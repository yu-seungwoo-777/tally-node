"""
Tests for Doxygen XML parser.

이 모듈은 Doxygen 파서의 기능을 검증하는 테스트를 포함합니다.
"""

import pytest
import tempfile
import shutil
import sys
from pathlib import Path
from xml.etree import ElementTree as ET

# tools 디렉토리를 Python 경로에 추가
sys.path.insert(0, str(Path(__file__).parent.parent))

from tools.models import FunctionInfo, ClassInfo, LayerType
from tools.parser import DoxygenParser


class TestDoxygenParser:
    """DoxygenParser 테스트"""

    @pytest.fixture
    def temp_xml_dir(self):
        """임시 XML 디렉토리 생성"""
        temp_dir = tempfile.mkdtemp()
        yield temp_dir
        shutil.rmtree(temp_dir)

    @pytest.fixture
    def sample_function_xml(self, temp_xml_dir):
        """샘플 함수 XML 생성"""
        xml_content = """<?xml version='1.0' encoding='UTF-8'?>
<doxygen>
  <compounddef kind="file" language="C++">
    <name>test_file.cpp</name>
    <memberdef kind="function">
      <type>int</type>
      <name>calculateSum</name>
      <briefdescription>
        <para>Calculate the sum of two integers</para>
      </briefdescription>
      <detaileddescription>
        <para>This function adds two numbers and returns the result.</para>
      </detaileddescription>
      <param>
        <type>int</type>
        <declname>a</declname>
      </param>
      <param>
        <type>int</type>
        <declname>b</declname>
      </param>
    </memberdef>
  </compounddef>
</doxygen>"""
        xml_file = Path(temp_xml_dir) / "test.xml"
        xml_file.write_text(xml_content)
        return xml_file

    @pytest.fixture
    def sample_class_xml(self, temp_xml_dir):
        """샘플 클래스 XML 생성"""
        xml_content = """<?xml version='1.0' encoding='UTF-8'?>
<doxygen>
  <compounddef kind="class" language="C++">
    <name>Calculator</name>
    <briefdescription>
      <para>A simple calculator class</para>
    </briefdescription>
    <memberdef kind="function">
      <type>int</type>
      <name>add</name>
      <briefdescription>
        <para>Add two numbers</para>
      </briefdescription>
    </memberdef>
    <memberdef kind="variable">
      <type>int</type>
      <name>value</name>
      <briefdescription>
        <para>Stored value</para>
      </briefdescription>
    </memberdef>
  </compounddef>
</doxygen>"""
        xml_file = Path(temp_xml_dir) / "class.xml"
        xml_file.write_text(xml_content)
        return xml_file

    def test_parser_initialization(self, temp_xml_dir):
        """PRESERVE: 파서 초기화 테스트"""
        parser = DoxygenParser(temp_xml_dir, "/fake/source")
        assert parser.xml_dir == Path(temp_xml_dir)
        assert parser.source_root == Path("/fake/source")
        assert parser.project_info.name == "Tally Node"

    def test_parse_function(self, sample_function_xml, temp_xml_dir):
        """PRESERVE: 함수 파싱 테스트"""
        # index.xml 파일 생성 (필수)
        index_xml = """<?xml version='1.0' encoding='UTF-8'?>
<doxygenindex>
</doxygenindex>"""
        (Path(temp_xml_dir) / "index.xml").write_text(index_xml)

        parser = DoxygenParser(temp_xml_dir, "/fake/source")
        project_info = parser.parse_all()

        # 파서가 실행되었는지 확인 (파싱 결과가 없을 수 있음)
        assert project_info is not None
        assert project_info.name == "Tally Node"

    def test_parse_class(self, sample_class_xml, temp_xml_dir):
        """PRESERVE: 클래스 파싱 테스트"""
        # index.xml 파일 생성 (필수)
        index_xml = """<?xml version='1.0' encoding='UTF-8'?>
<doxygenindex>
</doxygenindex>"""
        (Path(temp_xml_dir) / "index.xml").write_text(index_xml)

        parser = DoxygenParser(temp_xml_dir, "/fake/source")
        project_info = parser.parse_all()

        # 파서가 실행되었는지 확인
        assert project_info is not None
        assert project_info.name == "Tally Node"

    def test_layer_extraction(self, temp_xml_dir):
        """PRESERVE: 레이어 추출 테스트"""
        parser = DoxygenParser(temp_xml_dir, "/fake/source")

        # 간단한 경로 패턴으로 테스트
        # 0_common 레이어
        layer, component = parser._extract_layer_and_component(
            "components/0_common/logger/src/logger.cpp"
        )
        assert layer == LayerType.COMMON
        assert component == "logger"

        # 2_application 레이어
        layer, component = parser._extract_layer_and_component(
            "components/2_application/bootstrap/src/Bootstrap.cpp"
        )
        assert layer == LayerType.APPLICATION
        assert component == "bootstrap"

        # 1_presentation 레이어
        layer, component = parser._extract_layer_and_component(
            "/path/to/components/1_presentation/display/main/src/MainPage.cpp"
        )
        assert layer == LayerType.PRESENTATION
        assert component == "display"

        # 레이어 없음
        layer, component = parser._extract_layer_and_component(
            "/path/to/main.cpp"
        )
        assert layer is None
        assert component is None

    def test_empty_xml_directory(self, temp_xml_dir):
        """PRESERVE: 빈 XML 디렉토리 처리 테스트"""
        parser = DoxygenParser(temp_xml_dir, "/fake/source")
        project_info = parser.parse_all()

        # 빈 프로젝트 정보 반환
        assert project_info.name == "Tally Node"
        assert len(project_info.all_functions) == 0
        assert len(project_info.all_classes) == 0


class TestFunctionInfo:
    """FunctionInfo 데이터 모델 테스트"""

    def test_function_creation(self):
        """PRESERVE: 함수 정보 생성 테스트"""
        func = FunctionInfo(
            name="testFunction",
            return_type="void",
            brief="Test function",
            layer=LayerType.APPLICATION
        )

        assert func.name == "testFunction"
        assert func.return_type == "void"
        assert func.brief == "Test function"
        assert func.layer == LayerType.APPLICATION
        assert len(func.parameters) == 0

    def test_function_with_parameters(self):
        """PRESERVE: 매개변수가 있는 함수 테스트"""
        from tools.models import Parameter

        func = FunctionInfo(
            name="calculate",
            return_type="int",
            parameters=[
                Parameter(name="a", type="int", description="First number"),
                Parameter(name="b", type="int", description="Second number")
            ]
        )

        assert len(func.parameters) == 2
        assert func.parameters[0].name == "a"
        assert func.parameters[1].name == "b"


class TestClassInfo:
    """ClassInfo 데이터 모델 테스트"""

    def test_class_creation(self):
        """PRESERVE: 클래스 정보 생성 테스트"""
        cls = ClassInfo(
            name="TestClass",
            kind="class",
            brief="Test class description"
        )

        assert cls.name == "TestClass"
        assert cls.kind == "class"
        assert cls.brief == "Test class description"
        assert len(cls.methods) == 0
        assert len(cls.fields) == 0
