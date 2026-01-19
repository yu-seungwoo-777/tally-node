"""
Doxygen XML parser for C/C++ source code.

이 모듈은 Doxygen이 생성한 XML 파일을 파싱하여 구조화된 데이터를 추출합니다.
"""

import os
import xml.etree.ElementTree as ET
from typing import List, Optional, Dict
from pathlib import Path
import logging

from .models import (
    FunctionInfo, ClassInfo, MethodInfo, FieldInfo, Parameter,
    ComponentInfo, LayerInfo, ProjectInfo, LayerType
)

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class DoxygenParser:
    """Doxygen XML 파서"""

    def __init__(self, xml_dir: str, source_root: str):
        """
        초기화

        Args:
            xml_dir: Doxygen이 생성한 XML 디렉토리 경로
            source_root: 소스 코드 루트 디렉토리 경로
        """
        self.xml_dir = Path(xml_dir)
        self.source_root = Path(source_root)
        self.project_info = ProjectInfo(name="Tally Node")

    def parse_all(self) -> ProjectInfo:
        """
        모든 Doxygen XML 파일을 파싱

        Returns:
            ProjectInfo: 파싱된 프로젝트 정보
        """
        logger.info("Doxygen XML 파싱 시작...")

        # 인덱스 파일 파싱
        index_file = self.xml_dir / "index.xml"
        if not index_file.exists():
            logger.error(f"인덱스 파일을 찾을 수 없음: {index_file}")
            return self.project_info

        # 모든 XML 파일 수집
        xml_files = list(self.xml_dir.glob("*.xml"))
        logger.info(f"{len(xml_files)}개의 XML 파일 발견")

        for xml_file in xml_files:
            if xml_file.name == "index.xml":
                continue
            self._parse_xml_file(xml_file)

        # 레이어별로 분류
        self._classify_by_layers()

        logger.info(f"파싱 완료: {len(self.project_info.all_functions)}개 함수, "
                   f"{len(self.project_info.all_classes)}개 클래스")

        return self.project_info

    def _parse_xml_file(self, xml_file: Path):
        """단일 XML 파일 파싱"""
        try:
            tree = ET.parse(xml_file)
            root = tree.getroot()

            # compound 요소 처리
            for compound in root.findall(".//compound"):
                kind = compound.get("kind", "")
                if kind in ["class", "struct"]:
                    self._parse_class(compound)
                elif kind == "file":
                    self._parse_file(compound)

        except Exception as e:
            logger.warning(f"XML 파싱 실패 {xml_file}: {e}")

    def _parse_class(self, compound: ET.Element):
        """클래스/구조체 파싱"""
        compound_name = compound.findtext("name", "")
        if not compound_name:
            return

        class_info = ClassInfo(
            name=compound_name,
            kind=compound.get("kind", "class")
        )

        # 설명 파싱
        brief_desc = compound.find(".//briefdescription")
        if brief_desc is not None:
            class_info.brief = self._extract_text(brief_desc)

        detailed_desc = compound.find(".//detaileddescription")
        if detailed_desc is not None:
            class_info.detailed = self._extract_text(detailed_desc)

        # 위치 정보
        location = compound.find("location")
        if location is not None:
            file_path = location.get("file", "")
            class_info.file_path = file_path
            class_info.line_number = int(location.get("line", "0"))

            # 레이어와 컴포넌트 추출
            class_info.layer, class_info.component = self._extract_layer_and_component(file_path)

        # 멤버 파싱
        for member in compound.findall(".//member"):
            kind = member.get("kind", "")
            if kind == "function":
                method = self._parse_method(member)
                class_info.methods.append(method)
            elif kind == "variable":
                field = self._parse_field(member)
                class_info.fields.append(field)

        self.project_info.all_classes.append(class_info)

    def _parse_method(self, member: ET.Element) -> MethodInfo:
        """메서드 파싱"""
        method_info = MethodInfo(
            name=member.findtext("name", ""),
            return_type=member.findtext("type", "")
        )

        # 설명
        brief_desc = member.find("briefdescription")
        if brief_desc is not None:
            method_info.brief = self._extract_text(brief_desc)

        # 매개변수 파싱
        for param in member.findall(".//param"):
            param_name = param.findtext("declname", "")
            param_type = param.findtext("type", "")
            param_info = Parameter(name=param_name, type=param_type)
            method_info.parameters.append(param_info)

        # 반환값 설명
        for param in member.findall(".//param"):
            if param.findtext("declname", "") == "":
                # 반환값 설명
                method_info.return_description = self._extract_text(param)

        return method_info

    def _parse_field(self, member: ET.Element) -> FieldInfo:
        """필드 파싱"""
        field_info = FieldInfo(
            name=member.findtext("name", ""),
            type=member.findtext("type", "")
        )

        brief_desc = member.find("briefdescription")
        if brief_desc is not None:
            field_info.brief = self._extract_text(brief_desc)

        return field_info

    def _parse_file(self, compound: ET.Element):
        """파일 파싱 (최상위 함수 추출)"""
        file_path = compound.findtext("name", "")
        if not file_path:
            return

        # 레이어와 컴포넌트 추출
        layer, component = self._extract_layer_and_component(file_path)

        # 멤버 함수 파싱
        for member in compound.findall(".//member"):
            kind = member.get("kind", "")
            if kind == "function":
                func_info = self._parse_function(member, file_path, layer, component)
                self.project_info.all_functions.append(func_info)

    def _parse_function(self, member: ET.Element, file_path: str,
                       layer: Optional[LayerType], component: Optional[str]) -> FunctionInfo:
        """함수 파싱"""
        func_info = FunctionInfo(
            name=member.findtext("name", ""),
            return_type=member.findtext("type", ""),
            file_path=file_path,
            layer=layer,
            component=component
        )

        # 설명
        brief_desc = member.find("briefdescription")
        if brief_desc is not None:
            func_info.brief = self._extract_text(brief_desc)

        detailed_desc = member.find("detaileddescription")
        if detailed_desc is not None:
            func_info.detailed = self._extract_text(detailed_desc)

        # 매개변수
        for param in member.findall(".//param"):
            param_name = param.findtext("declname", "")
            param_type = param.findtext("type", "")

            # 매개변수 설명 추출
            param_desc = ""
            for para in param.findall(".//para"):
                param_desc += self._extract_text(para) + " "

            param_info = Parameter(
                name=param_name,
                type=param_type,
                description=param_desc.strip()
            )
            func_info.parameters.append(param_info)

        # 반환값 설명
        for param in member.findall(".//param"):
            if param.findtext("declname", "") == "" and param.findtext("type", "") == "":
                func_info.return_description = self._extract_text(param)

        # 위치 정보
        location = member.find("location")
        if location is not None:
            func_info.line_number = int(location.get("line", "0"))

        return func_info

    def _extract_text(self, element: ET.Element) -> str:
        """요소에서 텍스트 추출 (마크업 제거)"""
        if element is None:
            return ""

        # 간단한 텍스트 추출
        text = ET.tostring(element, encoding="unicode", method="text")
        return " ".join(text.split())

    def _extract_layer_and_component(self, file_path: str) -> tuple[Optional[LayerType], Optional[str]]:
        """파일 경로에서 레이어와 컴포넌트 추출"""
        try:
            path = Path(file_path)

            # 정규화된 경로 구분자 사용 (백슬래시를 슬래시로 변환)
            normalized_path = str(path).replace("\\", "/")

            # 경로를 부분으로 나누기
            parts = normalized_path.split("/")

            # 각 레이어 타입 확인
            for i, part in enumerate(parts):
                for layer in LayerType:
                    if part == layer.value:
                        # 레이어를 찾았음, 다음 부분이 컴포넌트
                        component = parts[i + 1] if i + 1 < len(parts) else None
                        return layer, component

            return None, None

        except Exception:
            return None, None

    def _classify_by_layers(self):
        """함수와 클래스를 레이어별로 분류"""
        layer_map: Dict[LayerType, LayerInfo] = {}

        # 초기화
        for layer in LayerType:
            layer_info = LayerInfo(
                layer_type=layer,
                name=layer.value.replace("_", " ").title()
            )
            layer_map[layer] = layer_info
            self.project_info.layers.append(layer_info)

        # 함수 분류
        for func in self.project_info.all_functions:
            if func.layer and func.layer in layer_map:
                layer_info = layer_map[func.layer]
                # 컴포넌트 찾기 또는 생성
                component = self._find_or_create_component(layer_info, func.component)
                component.functions.append(func)

        # 클래스 분류
        for cls in self.project_info.all_classes:
            if cls.layer and cls.layer in layer_map:
                layer_info = layer_map[cls.layer]
                component = self._find_or_create_component(layer_info, cls.component)
                component.classes.append(cls)

    def _find_or_create_component(self, layer_info: LayerInfo,
                                  component_name: Optional[str]) -> ComponentInfo:
        """컴포넌트 찾기 또는 생성"""
        if not component_name:
            # 기본 컴포넌트
            component_name = f"{layer_info.layer_type.value}_misc"

        for component in layer_info.components:
            if component.name == component_name:
                return component

        # 새 컴포넌트 생성
        new_component = ComponentInfo(
            name=component_name,
            layer=layer_info.layer_type
        )
        layer_info.components.append(new_component)
        return new_component
