"""
Tests for Markdown documentation generator.

이 모듈은 문서 생성기의 기능을 검증하는 테스트를 포함합니다.
"""

import pytest
import tempfile
import shutil
import sys
from pathlib import Path

# tools 디렉토리를 Python 경로에 추가
sys.path.insert(0, str(Path(__file__).parent.parent))

from tools.models import (
    ProjectInfo, LayerInfo, ComponentInfo,
    FunctionInfo, ClassInfo, LayerType, Parameter
)
from tools.generator import DocGeneratorSimple


class TestDocGeneratorSimple:
    """DocGeneratorSimple 테스트"""

    @pytest.fixture
    def temp_output_dir(self):
        """임시 출력 디렉토리 생성"""
        temp_dir = tempfile.mkdtemp()
        yield temp_dir
        shutil.rmtree(temp_dir)

    @pytest.fixture
    def sample_project_info(self):
        """샘플 프로젝트 정보 생성"""
        project = ProjectInfo(
            name="Test Project",
            brief="Test project for documentation"
        )

        # 레이어 추가
        common_layer = LayerInfo(
            layer_type=LayerType.COMMON,
            name="Common Layer",
            brief="Common utilities"
        )

        # 컴포넌트 추가
        logger_component = ComponentInfo(
            name="logger",
            layer=LayerType.COMMON,
            brief="Logging utility"
        )

        # 함수 추가
        log_func = FunctionInfo(
            name="logMessage",
            return_type="void",
            brief="Log a message",
            layer=LayerType.COMMON,
            component="logger",
            parameters=[
                Parameter(name="message", type="const char*", description="Message to log")
            ]
        )
        logger_component.functions.append(log_func)

        # 클래스 추가
        logger_class = ClassInfo(
            name="Logger",
            kind="class",
            brief="Logger class",
            layer=LayerType.COMMON,
            component="logger"
        )
        logger_component.classes.append(logger_class)

        common_layer.components.append(logger_component)
        project.layers.append(common_layer)
        project.all_functions.append(log_func)
        project.all_classes.append(logger_class)

        return project

    def test_generator_initialization(self, temp_output_dir):
        """PRESERVE: 생성기 초기화 테스트"""
        generator = DocGeneratorSimple(temp_output_dir)
        assert generator.output_dir == Path(temp_output_dir)
        assert generator.output_dir.exists()

    def test_generate_api_docs(self, temp_output_dir, sample_project_info):
        """PRESERVE: API 문서 생성 테스트"""
        generator = DocGeneratorSimple(temp_output_dir)
        generator._generate_api_docs_simple(sample_project_info)

        api_dir = Path(temp_output_dir) / "api"
        assert api_dir.exists()

        functions_md = api_dir / "functions.md"
        classes_md = api_dir / "classes.md"

        assert functions_md.exists()
        assert classes_md.exists()

        # 내용 확인
        functions_content = functions_md.read_text(encoding="utf-8")
        assert "logMessage" in functions_content
        assert "Test Project" in functions_content

        classes_content = classes_md.read_text(encoding="utf-8")
        assert "Logger" in classes_content

    def test_generate_layer_docs(self, temp_output_dir, sample_project_info):
        """PRESERVE: 레이어 문서 생성 테스트"""
        generator = DocGeneratorSimple(temp_output_dir)
        generator._generate_layer_docs_simple(sample_project_info)

        layer_dir = Path(temp_output_dir) / "layers"
        assert layer_dir.exists()

        common_md = layer_dir / "0_common.md"
        assert common_md.exists()

        content = common_md.read_text(encoding="utf-8")
        assert "Common Layer" in content
        assert "logger" in content

    def test_generate_index(self, temp_output_dir, sample_project_info):
        """PRESERVE: 인덱스 문서 생성 테스트"""
        generator = DocGeneratorSimple(temp_output_dir)
        generator._generate_index_simple(sample_project_info)

        readme = Path(temp_output_dir) / "README.md"
        assert readme.exists()

        content = readme.read_text(encoding="utf-8")
        assert "Test Project" in content
        assert "API Reference" in content
        assert "Architecture" in content

    def test_generate_all(self, temp_output_dir, sample_project_info):
        """PRESERVE: 전체 문서 생성 통합 테스트"""
        generator = DocGeneratorSimple(temp_output_dir)
        generator.generate_all(sample_project_info)

        # 모든 파일이 생성되었는지 확인
        assert (Path(temp_output_dir) / "README.md").exists()
        assert (Path(temp_output_dir) / "api" / "functions.md").exists()
        assert (Path(temp_output_dir) / "api" / "classes.md").exists()
        assert (Path(temp_output_dir) / "layers" / "0_common.md").exists()

    def test_function_markdown_formatting(self, temp_output_dir):
        """PRESERVE: 함수 마크다운 포맷팅 테스트"""
        project = ProjectInfo(name="Test")

        func = FunctionInfo(
            name="complexFunction",
            return_type="bool",
            brief="A complex function with multiple parameters",
            detailed="This function demonstrates various markdown formatting features.",
            parameters=[
                Parameter(name="param1", type="int", description="First parameter"),
                Parameter(name="param2", type="float", description="Second parameter"),
                Parameter(name="param3", type="const char*", description="Third parameter")
            ],
            return_description="True on success, false on failure",
            layer=LayerType.APPLICATION,
            component="test_component"
        )

        project.all_functions.append(func)

        generator = DocGeneratorSimple(temp_output_dir)
        generator._generate_api_docs_simple(project)

        functions_md = Path(temp_output_dir) / "api" / "functions.md"
        content = functions_md.read_text(encoding="utf-8")

        # 마크다운 형식 확인
        assert "### complexFunction" in content
        assert "```cpp" in content
        assert "bool complexFunction(int param1, float param2, const char* param3)" in content
        assert "**Parameters:**" in content
        assert "**Returns:**" in content

    def test_class_markdown_formatting(self, temp_output_dir):
        """PRESERVE: 클래스 마크다운 포맷팅 테스트"""
        from tools.models import MethodInfo, FieldInfo

        project = ProjectInfo(name="Test")

        cls = ClassInfo(
            name="ComplexClass",
            kind="class",
            brief="A class with methods and fields",
            detailed="This class demonstrates various class documentation features.",
            layer=LayerType.DOMAIN,
            component="test_component"
        )

        # 메서드 추가
        method = MethodInfo(
            name="doSomething",
            return_type="void",
            brief="Do something useful"
        )
        cls.methods.append(method)

        # 필드 추가
        field = FieldInfo(
            name="value",
            type="int",
            brief="Stored value"
        )
        cls.fields.append(field)

        project.all_classes.append(cls)

        generator = DocGeneratorSimple(temp_output_dir)
        generator._generate_api_docs_simple(project)

        classes_md = Path(temp_output_dir) / "api" / "classes.md"
        content = classes_md.read_text(encoding="utf-8")

        # 마크다운 형식 확인
        assert "### ComplexClass" in content
        assert "#### Methods" in content
        assert "#### Fields" in content
        assert "doSomething" in content
        assert "value" in content


class TestMarkdownFormatting:
    """마크다운 형식 테스트"""

    def test_code_block_formatting(self):
        """PRESERVE: 코드 블록 포맷팅 테스트"""
        generator = DocGeneratorSimple("/tmp/test")
        code = "int main() { return 0; }"
        formatted = generator._format_code(code)

        assert formatted == "```cpp\nint main() { return 0; }\n```"

    def test_escape_underscores(self):
        """PRESERVE: 언더스코어 이스케이프 테스트"""
        generator = DocGeneratorSimple("/tmp/test")
        text = "my_variable_name"
        escaped = generator._escape_underscores(text)

        assert escaped == "my\\_variable\\_name"
