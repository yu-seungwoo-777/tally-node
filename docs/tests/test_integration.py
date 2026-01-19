"""
Integration tests for documentation generation system.

이 모듈은 전체 문서 생성 파이프라인의 통합 테스트를 포함합니다.
"""

import pytest
import tempfile
import shutil
import sys
from pathlib import Path

# tools 디렉토리를 Python 경로에 추가
sys.path.insert(0, str(Path(__file__).parent.parent))

from tools.scan_project import ProjectScanner
from tools.parser import DoxygenParser
from tools.generator import DocGeneratorSimple


class TestDocumentationPipeline:
    """문서 생성 파이프라인 통합 테스트"""

    @pytest.fixture
    def temp_source_dir(self):
        """임시 소스 디렉토리 생성"""
        temp_dir = tempfile.mkdtemp()
        source_path = Path(temp_dir) / "components" / "2_application" / "test_component"
        source_path.mkdir(parents=True)

        # 샘플 C++ 파일 생성
        cpp_file = source_path / "test.cpp"
        cpp_content = """/**
 * @file test.cpp
 * @brief Test component implementation
 */

/**
 * @brief Calculate the sum of two numbers
 *
 * This function adds two integers and returns the result.
 *
 * @param a First number
 * @param b Second number
 * @return Sum of a and b
 */
int calculateSum(int a, int b) {
    return a + b;
}

/**
 * @brief Test class for demonstration
 */
class TestClass {
public:
    /**
     * @brief Constructor
     */
    TestClass() : value(0) {}

    /**
     * @brief Get the value
     * @return Current value
     */
    int getValue() const { return value; }

private:
    int value; ///< Stored value
};
"""
        cpp_file.write_text(cpp_content)

        # 헤더 파일 생성
        h_file = source_path / "test.h"
        h_content = """/**
 * @file test.h
 * @brief Test component header
 */

#ifndef TEST_H
#define TEST_H

int calculateSum(int a, int b);

class TestClass {
public:
    TestClass();
    int getValue() const;

private:
    int value;
};

#endif // TEST_H
"""
        h_file.write_text(h_content)

        yield temp_dir
        shutil.rmtree(temp_dir)

    @pytest.fixture
    def temp_output_dir(self):
        """임시 출력 디렉토리 생성"""
        temp_dir = tempfile.mkdtemp()
        yield temp_dir
        shutil.rmtree(temp_dir)

    @pytest.mark.slow
    def test_full_pipeline_with_doxygen(self, temp_source_dir, temp_output_dir):
        """PRESERVE: Doxygen을 포함한 전체 파이프라인 테스트"""
        # Doxygen 실행 (시스템에 doxygen이 설치되어 있어야 함)
        scanner = ProjectScanner(temp_source_dir, temp_output_dir)
        doxygen_success = scanner.scan()

        # Doxygen이 설치되지 않은 경우 테스트 스킵
        if not doxygen_success:
            pytest.skip("Doxygen not installed or failed")

        # XML 파싱
        doxygen_xml_dir = Path(temp_output_dir) / "doxygen_xml"
        parser = DoxygenParser(str(doxygen_xml_dir), temp_source_dir)
        project_info = parser.parse_all()

        # 파싱 결과 확인
        assert project_info.name == "Tally Node"

        # 문서 생성
        generator = DocGeneratorSimple(str(Path(temp_output_dir) / "markdown"))
        generator.generate_all(project_info)

        # 출력 파일 확인
        markdown_dir = Path(temp_output_dir) / "markdown"
        assert (markdown_dir / "README.md").exists()
        assert (markdown_dir / "api" / "functions.md").exists()
        assert (markdown_dir / "api" / "classes.md").exists()

    def test_parser_to_generator_pipeline(self, temp_output_dir):
        """PRESERVE: 파서에서 생성기까지의 파이프라인 테스트 (Doxygen 없이)"""
        from tools.models import ProjectInfo, LayerInfo, ComponentInfo, FunctionInfo, LayerType

        # 가상의 파싱 결과 생성
        project_info = ProjectInfo(name="Test Pipeline")

        layer_info = LayerInfo(
            layer_type=LayerType.APPLICATION,
            name="Application Layer"
        )

        component_info = ComponentInfo(
            name="test_component",
            layer=LayerType.APPLICATION,
            brief="Test component"
        )

        function_info = FunctionInfo(
            name="pipelineFunction",
            return_type="void",
            brief="Test function for pipeline",
            layer=LayerType.APPLICATION,
            component="test_component"
        )

        component_info.functions.append(function_info)
        layer_info.components.append(component_info)
        project_info.layers.append(layer_info)
        project_info.all_functions.append(function_info)

        # 문서 생성
        generator = DocGeneratorSimple(str(Path(temp_output_dir) / "output"))
        generator.generate_all(project_info)

        # 결과 확인
        output_dir = Path(temp_output_dir) / "output"
        assert (output_dir / "README.md").exists()

        readme_content = (output_dir / "README.md").read_text(encoding="utf-8")
        assert "Test Pipeline" in readme_content
        assert "pipelineFunction" in readme_content or "Application Layer" in readme_content

    def test_empty_project_handling(self, temp_output_dir):
        """PRESERVE: 빈 프로젝트 처리 테스트"""
        from tools.models import ProjectInfo

        project_info = ProjectInfo(name="Empty Project")

        generator = DocGeneratorSimple(str(Path(temp_output_dir) / "output"))
        generator.generate_all(project_info)

        # 빈 프로젝트도 문서가 생성되어야 함
        output_dir = Path(temp_output_dir) / "output"
        assert (output_dir / "README.md").exists()

    def test_multi_layer_documentation(self, temp_output_dir):
        """PRESERVE: 다중 레이어 문서화 테스트"""
        from tools.models import ProjectInfo, LayerInfo, LayerType

        project_info = ProjectInfo(name="Multi Layer Test")

        # 모든 레이어 추가
        for layer_type in LayerType:
            layer_info = LayerInfo(
                layer_type=layer_type,
                name=layer_type.value.replace("_", " ").title()
            )
            project_info.layers.append(layer_info)

        # 문서 생성
        generator = DocGeneratorSimple(str(Path(temp_output_dir) / "output"))
        generator.generate_all(project_info)

        # 모든 레이어에 대한 문서가 생성되어야 함
        output_dir = Path(temp_output_dir) / "output" / "layers"
        assert output_dir.exists()

        for layer_type in LayerType:
            layer_file = output_dir / f"{layer_type.value}.md"
            assert layer_file.exists(), f"Layer file not found: {layer_file}"


class TestProjectScanner:
    """ProjectScanner 테스트"""

    @pytest.fixture
    def temp_source_dir(self):
        """임시 소스 디렉토리 생성"""
        temp_dir = tempfile.mkdtemp()
        yield temp_dir
        shutil.rmtree(temp_dir)

    @pytest.fixture
    def temp_output_dir(self):
        """임시 출력 디렉토리 생성"""
        temp_dir = tempfile.mkdtemp()
        yield temp_dir
        shutil.rmtree(temp_dir)

    def test_scanner_initialization(self, temp_source_dir, temp_output_dir):
        """PRESERVE: 스캐너 초기화 테스트"""
        scanner = ProjectScanner(temp_source_dir, temp_output_dir)

        assert scanner.source_root == Path(temp_source_dir)
        assert scanner.output_dir == Path(temp_output_dir)
        assert scanner.doxygen_output.exists()

    def test_doxygen_file_generation(self, temp_source_dir, temp_output_dir):
        """PRESERVE: Doxygen 설정 파일 생성 테스트"""
        scanner = ProjectScanner(temp_source_dir, temp_output_dir)
        doxyfile = scanner._generate_doxyfile()

        assert doxyfile.exists()
        assert doxyfile.name == "Doxyfile"

        content = doxyfile.read_text(encoding="utf-8")
        assert "PROJECT_NAME" in content
        # GENERATE_XML이 설정되어 있는지 확인 (공백 처리)
        assert "GENERATE_XML" in content and "= YES" in content

    @pytest.mark.slow
    def test_scan_execution(self, temp_source_dir, temp_output_dir):
        """PRESERVE: 스캔 실행 테스트 (Doxygen 필요)"""
        # 샘플 파일 생성
        (Path(temp_source_dir) / "test.cpp").write_text("// test file")

        scanner = ProjectScanner(temp_source_dir, temp_output_dir)
        success = scanner.scan()

        # Doxygen이 설치되지 않은 경우
        if not success:
            pytest.skip("Doxygen not installed")

        assert scanner.doxygen_output.exists()
        assert any(scanner.doxygen_output.iterdir())


class TestErrorHandling:
    """오류 처리 테스트"""

    def test_missing_xml_directory(self):
        """PRESERVE: 존재하지 않는 XML 디렉토리 처리 테스트"""
        parser = DoxygenParser("/nonexistent/xml", "/fake/source")
        project_info = parser.parse_all()

        # 빈 프로젝트 정보를 반환해야 함
        assert project_info.name == "Tally Node"
        assert len(project_info.all_functions) == 0
        assert len(project_info.all_classes) == 0

    def test_invalid_xml_content(self, tmp_path):
        """PRESERVE: 잘못된 XML 내용 처리 테스트"""
        # 잘못된 XML 파일 생성
        invalid_xml = tmp_path / "invalid.xml"
        invalid_xml.write_text("This is not valid XML")

        parser = DoxygenParser(str(tmp_path), "/fake/source")
        # 예외가 발생하지 않아야 함
        project_info = parser.parse_all()

        assert project_info is not None
