"""
Project scanner for documentation generation.

이 모듈은 프로젝트를 스캔하고 Doxygen을 실행하여 XML을 생성합니다.
"""

import os
import subprocess
import shutil
from pathlib import Path
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class ProjectScanner:
    """프로젝트 스캐너"""

    def __init__(self, source_root: str, output_dir: str):
        """
        초기화

        Args:
            source_root: 소스 코드 루트 디렉토리
            output_dir: 출력 디렉토리
        """
        self.source_root = Path(source_root)
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)

        # Doxygen 출력 디렉토리
        self.doxygen_output = self.output_dir / "doxygen_xml"
        self.doxygen_output.mkdir(exist_ok=True)

    def scan(self) -> bool:
        """
        프로젝트 스캔 및 Doxygen 실행

        Returns:
            성공 여부
        """
        logger.info(f"프로젝트 스캔 시작: {self.source_root}")

        # Doxygen 설정 파일 생성
        doxyfile = self._generate_doxyfile()

        # Doxygen 실행
        success = self._run_doxygen(doxyfile)

        if success:
            logger.info(f"Doxygen XML 생성 완료: {self.doxygen_output}")
        else:
            logger.error("Doxygen 실행 실패")

        return success

    def _generate_doxyfile(self) -> Path:
        """Doxygen 설정 파일 생성"""
        doxyfile_path = self.output_dir / "Doxyfile"

        # Doxygen 설정
        config = f"""
# Doxygen 설정 파일 (자동 생성)

PROJECT_NAME           = "Tally Node"
PROJECT_BRIEF          = "Tally System with LoRa Communication"

# 출력 설정
OUTPUT_DIRECTORY       = "{self.doxygen_output}"
GENERATE_HTML          = NO
GENERATE_LATEX         = NO
GENERATE_XML           = YES
XML_OUTPUT             = "{self.doxygen_output}"
XML_PROGRAMLISTING     = YES

# 입력 설정
INPUT                  = {" ".join(self._get_input_paths())}
RECURSIVE              = YES
EXCLUDE_PATTERNS       = */.pio/* */.git/* */venv/* */build/*

# 파싱 설정
EXTRACT_ALL            = YES
EXTRACT_PRIVATE        = NO
EXTRACT_STATIC         = YES
EXTRACT_LOCAL_CLASSES  = YES

# 언어 설정
OPTIMIZE_OUTPUT_JAVA   = NO
OPTIMIZE_FOR_C         = YES

# 참고 설정
REFERENCES_LINK_SOURCE = YES
SOURCE_BROWSER         = YES

# 다이어그램
HAVE_DOT               = NO
CLASS_DIAGRAMS         = YES
"""

        doxyfile_path.write_text(config, encoding="utf-8")
        logger.info(f"Doxygen 설정 파일 생성: {doxyfile_path}")

        return doxyfile_path

    def _get_input_paths(self) -> list[str]:
        """입력 경로 목록 반환"""
        input_paths = []

        # components 디렉토리 찾기
        for components_dir in self.source_root.glob("**/components"):
            if components_dir.is_dir():
                input_paths.append(str(components_dir))

        # main 소스 파일들
        for src_file in self.source_root.glob("*.cpp"):
            input_paths.append(str(src_file))

        for src_file in self.source_root.glob("*.c"):
            input_paths.append(str(src_file))

        if not input_paths:
            # 기본 경로
            input_paths = [str(self.source_root)]

        logger.info(f"입력 경로: {len(input_paths)}개")
        return input_paths

    def _run_doxygen(self, doxyfile: Path) -> bool:
        """
        Doxygen 실행

        Args:
            doxyfile: Doxygen 설정 파일 경로

        Returns:
            성공 여부
        """
        try:
            # doxygen 설치 확인
            if not shutil.which("doxygen"):
                logger.error("Doxygen이 설치되지 않았습니다")
                return False

            # Doxygen 실행
            result = subprocess.run(
                ["doxygen", str(doxyfile)],
                capture_output=True,
                text=True,
                timeout=300  # 5분 타임아웃
            )

            if result.returncode == 0:
                logger.info("Doxygen 실행 성공")
                return True
            else:
                logger.error(f"Doxygen 실행 실패: {result.stderr}")
                return False

        except subprocess.TimeoutExpired:
            logger.error("Doxygen 실행 시간 초과")
            return False
        except Exception as e:
            logger.error(f"Doxygen 실행 중 오류: {e}")
            return False


def main():
    """메인 함수"""
    import sys

    if len(sys.argv) < 2:
        print("Usage: python scan_project.py <source_root> [output_dir]")
        sys.exit(1)

    source_root = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "docs_output"

    scanner = ProjectScanner(source_root, output_dir)
    success = scanner.scan()

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
