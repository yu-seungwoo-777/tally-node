"""
Markdown documentation generator using Jinja2 templates.

이 모듈은 파싱된 데이터를 바탕으로 Markdown 문서를 생성합니다.
"""

import os
from pathlib import Path
from typing import Dict, Any
from jinja2 import Environment, FileSystemLoader, Template
import logging

from .models import (
    ProjectInfo, LayerInfo, ComponentInfo,
    FunctionInfo, ClassInfo, LayerType
)

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class DocGenerator:
    """Markdown 문서 생성기"""

    def __init__(self, template_dir: str, output_dir: str):
        """
        초기화

        Args:
            template_dir: Jinja2 템플릿 디렉토리 경로
            output_dir: 출력 디렉토리 경로
        """
        self.template_dir = Path(template_dir)
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)

        # Jinja2 환경 설정
        self.env = Environment(
            loader=FileSystemLoader(str(self.template_dir)),
            trim_blocks=True,
            lstrip_blocks=True
        )

        # 커스텀 필터 등록
        self.env.filters['format_code'] = self._format_code
        self.env.filters['escape_underscores'] = self._escape_underscores

    def generate_all(self, project_info: ProjectInfo):
        """
        모든 문서 생성

        Args:
            project_info: 파싱된 프로젝트 정보
        """
        logger.info("문서 생성 시작...")

        # API 문서 생성
        self._generate_api_docs(project_info)

        # 레이어 문서 생성
        self._generate_layer_docs(project_info)

        # 컴포넌트 문서 생성
        self._generate_component_docs(project_info)

        logger.info(f"문서 생성 완료: {self.output_dir}")

    def _generate_api_docs(self, project_info: ProjectInfo):
        """API 문서 생성"""
        api_dir = self.output_dir / "api"
        api_dir.mkdir(exist_ok=True)

        # 함수 API 문서
        functions_md = self._generate_functions_md(project_info)
        (api_dir / "functions.md").write_text(functions_md, encoding="utf-8")

        # 클래스 API 문서
        classes_md = self._generate_classes_md(project_info)
        (api_dir / "classes.md").write_text(classes_md, encoding="utf-8")

        logger.info(f"API 문서 생성 완료: {api_dir}")

    def _generate_functions_md(self, project_info: ProjectInfo) -> str:
        """함수 API 문서 생성"""
        template = self.env.get_template("api_function.md.j2")

        # 레이어별로 함수 그룹화
        functions_by_layer: Dict[LayerType, list[FunctionInfo]] = {}
        for layer in LayerType:
            functions_by_layer[layer] = [
                f for f in project_info.all_functions if f.layer == layer
            ]

        return template.render(
            project_name=project_info.name,
            functions_by_layer=functions_by_layer,
            layers=project_info.layers
        )

    def _generate_classes_md(self, project_info: ProjectInfo) -> str:
        """클래스 API 문서 생성"""
        template = self.env.get_template("api_class.md.j2")

        # 레이어별로 클래스 그룹화
        classes_by_layer: Dict[LayerType, list[ClassInfo]] = {}
        for layer in LayerType:
            classes_by_layer[layer] = [
                c for c in project_info.all_classes if c.layer == layer
            ]

        return template.render(
            project_name=project_info.name,
            classes_by_layer=classes_by_layer,
            layers=project_info.layers
        )

    def _generate_layer_docs(self, project_info: ProjectInfo):
        """레이어 문서 생성"""
        layer_dir = self.output_dir / "layers"
        layer_dir.mkdir(exist_ok=True)

        for layer_info in project_info.layers:
            layer_md = self._generate_layer_md(layer_info, project_info)
            filename = f"{layer_info.layer_type.value}.md"
            (layer_dir / filename).write_text(layer_md, encoding="utf-8")

        logger.info(f"레이어 문서 생성 완료: {layer_dir}")

    def _generate_layer_md(self, layer_info: LayerInfo, project_info: ProjectInfo) -> str:
        """단일 레이어 문서 생성"""
        template = self.env.get_template("layer.md.j2")

        # 의존성 정보 수집
        dependencies = []
        for dep_layer in layer_info.dependencies:
            dep_info = next((l for l in project_info.layers if l.layer_type == dep_layer), None)
            if dep_info:
                dependencies.append(dep_info)

        return template.render(
            layer=layer_info,
            project=project_info,
            dependencies=dependencies
        )

    def _generate_component_docs(self, project_info: ProjectInfo):
        """컴포넌트 문서 생성"""
        component_dir = self.output_dir / "components"
        component_dir.mkdir(exist_ok=True)

        for layer_info in project_info.layers:
            for component in layer_info.components:
                component_md = self._generate_component_md(component, layer_info)
                filename = f"{component.layer.value}_{component.name}.md"
                (component_dir / filename).write_text(component_md, encoding="utf-8")

        logger.info(f"컴포넌트 문서 생성 완료: {component_dir}")

    def _generate_component_md(self, component: ComponentInfo, layer_info: LayerInfo) -> str:
        """단일 컴포넌트 문서 생성"""
        template = self.env.get_template("component.md.j2")

        return template.render(
            component=component,
            layer=layer_info
        )

    @staticmethod
    def _format_code(code: str) -> str:
        """코드 포맷팅 필터"""
        return f"```cpp\n{code}\n```"

    @staticmethod
    def _escape_underscores(text: str) -> str:
        """마크다운 백틱 내에서 언더스코어 이스케이프"""
        return text.replace("_", "\\_")


class DocGeneratorSimple:
    """단순 템플릿 없는 문서 생성기"""

    def __init__(self, output_dir: str):
        """
        초기화

        Args:
            output_dir: 출력 디렉토리 경로
        """
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)

    def generate_all(self, project_info: ProjectInfo):
        """
        모든 문서 생성 (템플릿 없이 직접 생성)

        Args:
            project_info: 파싱된 프로젝트 정보
        """
        logger.info("문서 생성 시작 (단순 모드)...")

        # API 문서 생성
        self._generate_api_docs_simple(project_info)

        # 레이어 문서 생성
        self._generate_layer_docs_simple(project_info)

        # 인덱스 문서 생성
        self._generate_index_simple(project_info)

        logger.info(f"문서 생성 완료: {self.output_dir}")

    def _generate_api_docs_simple(self, project_info: ProjectInfo):
        """API 문서 생성 (단순 모드)"""
        api_dir = self.output_dir / "api"
        api_dir.mkdir(exist_ok=True)

        # 함수 목록
        functions_md = self._build_functions_md(project_info)
        (api_dir / "functions.md").write_text(functions_md, encoding="utf-8")

        # 클래스 목록
        classes_md = self._build_classes_md(project_info)
        (api_dir / "classes.md").write_text(classes_md, encoding="utf-8")

    def _build_functions_md(self, project_info: ProjectInfo) -> str:
        """함수 문서 빌드"""
        lines = [
            f"# API Functions Reference\n",
            f"**Project**: {project_info.name}\n",
            f"## Overview\n",
            f"This document describes all public functions in the project.\n"
        ]

        # 레이어별로 그룹화
        for layer in LayerType:
            layer_functions = [f for f in project_info.all_functions if f.layer == layer]
            if not layer_functions:
                continue

            lines.append(f"\n## {layer.value.replace('_', ' ').title()}\n")

            for func in layer_functions:
                lines.append(self._build_function_section(func))

        return "\n".join(lines)

    def _build_function_section(self, func: FunctionInfo) -> str:
        """단일 함수 섹션 빌드"""
        lines = [
            f"\n### {func.name}\n"
        ]

        if func.brief:
            lines.append(f"{func.brief}\n")

        # 시그니처
        params_str = ", ".join([f"{p.type} {p.name}" for p in func.parameters])
        signature = f"{func.return_type} {func.name}({params_str})"
        lines.append(f"```cpp\n{signature}\n```\n")

        # 매개변수
        if func.parameters:
            lines.append(f"\n**Parameters:**\n")
            for param in func.parameters:
                desc = f": {param.description}" if param.description else ""
                lines.append(f"- `{param.type} {param.name}`{desc}")
            lines.append("")

        # 반환값
        if func.return_description:
            lines.append(f"\n**Returns:** {func.return_description}\n")

        # 상세 설명
        if func.detailed:
            lines.append(f"\n{func.detailed}\n")

        # 메타데이터
        if func.component:
            lines.append(f"\n*Component: {func.component}*")
        if func.file_path:
            lines.append(f"*Location: {func.file_path}:{func.line_number}*")

        return "\n".join(lines)

    def _build_classes_md(self, project_info: ProjectInfo) -> str:
        """클래스 문서 빌드"""
        lines = [
            f"# API Classes Reference\n",
            f"**Project**: {project_info.name}\n",
            f"## Overview\n",
            f"This document describes all classes and structs in the project.\n"
        ]

        # 레이어별로 그룹화
        for layer in LayerType:
            layer_classes = [c for c in project_info.all_classes if c.layer == layer]
            if not layer_classes:
                continue

            lines.append(f"\n## {layer.value.replace('_', ' ').title()}\n")

            for cls in layer_classes:
                lines.append(self._build_class_section(cls))

        return "\n".join(lines)

    def _build_class_section(self, cls: ClassInfo) -> str:
        """단일 클래스 섹션 빌드"""
        lines = [
            f"\n### {cls.name}\n"
        ]

        if cls.brief:
            lines.append(f"{cls.brief}\n")

        # 상세 설명
        if cls.detailed:
            lines.append(f"\n{cls.detailed}\n")

        # 기본 클래스
        if cls.base_classes:
            lines.append(f"\n**Inherits:** {', '.join(cls.base_classes)}\n")

        # 메서드
        if cls.methods:
            lines.append(f"\n#### Methods\n")
            for method in cls.methods:
                params_str = ", ".join([f"{p.type} {p.name}" for p in method.parameters])
                signature = f"{method.return_type} {method.name}({params_str})"
                lines.append(f"- ```cpp\n{signature}\n```")
                if method.brief:
                    lines.append(f"  - {method.brief}")

        # 필드
        if cls.fields:
            lines.append(f"\n#### Fields\n")
            for field in cls.fields:
                lines.append(f"- `{field.type} {field.name}`")
                if field.brief:
                    lines.append(f"  - {field.brief}")

        # 메타데이터
        if cls.component:
            lines.append(f"\n*Component: {cls.component}*")
        if cls.file_path:
            lines.append(f"*Location: {cls.file_path}:{cls.line_number}*")

        return "\n".join(lines)

    def _generate_layer_docs_simple(self, project_info: ProjectInfo):
        """레이어 문서 생성 (단순 모드)"""
        layer_dir = self.output_dir / "layers"
        layer_dir.mkdir(exist_ok=True)

        for layer_info in project_info.layers:
            layer_md = self._build_layer_md(layer_info)
            filename = f"{layer_info.layer_type.value}.md"
            (layer_dir / filename).write_text(layer_md, encoding="utf-8")

    def _build_layer_md(self, layer_info: LayerInfo) -> str:
        """단일 레이어 문서 빌드"""
        lines = [
            f"# {layer_info.name}\n",
            f"{layer_info.brief}\n" if layer_info.brief else "",
            f"\n## Components\n"
        ]

        for component in layer_info.components:
            lines.append(f"\n### {component.name}\n")
            if component.brief:
                lines.append(f"{component.brief}\n")

            # 통계
            func_count = len(component.functions)
            class_count = len(component.classes)
            lines.append(f"- Functions: {func_count}")
            lines.append(f"- Classes: {class_count}")

        return "\n".join(lines)

    def _generate_index_simple(self, project_info: ProjectInfo):
        """인덱스 문서 생성"""
        lines = [
            f"# {project_info.name} Documentation\n",
            f"\n## API Reference\n",
            f"- [Functions](api/functions.md)\n",
            f"- [Classes](api/classes.md)\n",
            f"\n## Architecture\n",
        ]

        for layer_info in project_info.layers:
            filename = f"layers/{layer_info.layer_type.value}.md"
            lines.append(f"- [{layer_info.name}]({filename})\n")

        (self.output_dir / "README.md").write_text("\n".join(lines), encoding="utf-8")

    @staticmethod
    def _format_code(code: str) -> str:
        """코드 포맷팅 필터"""
        return f"```cpp\n{code}\n```"

    @staticmethod
    def _escape_underscores(text: str) -> str:
        """마크다운 백틱 내에서 언더스코어 이스케이프"""
        return text.replace("_", "\\_")
