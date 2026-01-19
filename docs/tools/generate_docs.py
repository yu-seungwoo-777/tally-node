#!/usr/bin/env python3
"""
Main script for automatic documentation generation.

이 스크립트는 전체 문서 생성 파이프라인을 실행합니다:
1. Doxygen으로 XML 생성
2. XML 파싱
3. Markdown 문서 생성
"""

import sys
import argparse
import logging
from pathlib import Path

# 현재 디렉토리를 Python 경로에 추가
sys.path.insert(0, str(Path(__file__).parent))

from scan_project import ProjectScanner
from parser import DoxygenParser
from generator import DocGeneratorSimple

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


def main():
    """메인 함수"""
    parser = argparse.ArgumentParser(
        description="자동 문서 생성 시스템"
    )
    parser.add_argument(
        "source_root",
        help="소스 코드 루트 디렉토리"
    )
    parser.add_argument(
        "-o", "--output",
        default="docs/output",
        help="출력 디렉토리 (기본값: docs/output)"
    )
    parser.add_argument(
        "--skip-doxygen",
        action="store_true",
        help="Doxygen 실행 건너뛰기 (이미 XML이 있는 경우)"
    )

    args = parser.parse_args()

    logger.info("=" * 60)
    logger.info("자동 문서 생성 시스템 시작")
    logger.info("=" * 60)

    source_root = Path(args.source_root)
    output_dir = Path(args.output)

    if not source_root.exists():
        logger.error(f"소스 디렉토리를 찾을 수 없음: {source_root}")
        return 1

    # 단계 1: Doxygen 실행 (필요한 경우)
    if not args.skip_doxygen:
        logger.info("\n[1/3] Doxygen 실행 중...")
        scanner = ProjectScanner(str(source_root), str(output_dir))
        if not scanner.scan():
            logger.error("Doxygen 실행 실패")
            return 1
    else:
        logger.info("\n[1/3] Doxygen 실행 건너뜀")

    doxygen_xml_dir = output_dir / "doxygen_xml"

    # 단계 2: XML 파싱
    logger.info("\n[2/3] XML 파싱 중...")
    parser_obj = DoxygenParser(str(doxygen_xml_dir), str(source_root))
    project_info = parser_obj.parse_all()

    if not project_info.all_functions and not project_info.all_classes:
        logger.warning("파싱된 함수 또는 클래스가 없습니다")
        logger.warning("소스 코드에 Doxygen 스타일 주석이 있는지 확인하세요")

    # 단계 3: 문서 생성
    logger.info("\n[3/3] Markdown 문서 생성 중...")
    generator = DocGeneratorSimple(str(output_dir / "markdown"))
    generator.generate_all(project_info)

    logger.info("\n" + "=" * 60)
    logger.info("문서 생성 완료!")
    logger.info(f"출력 위치: {output_dir / 'markdown'}")
    logger.info("=" * 60)

    # 통계 출력
    logger.info(f"\n통계:")
    logger.info(f"  - 함수: {len(project_info.all_functions)}개")
    logger.info(f"  - 클래스: {len(project_info.all_classes)}개")
    logger.info(f"  - 레이어: {len(project_info.layers)}개")

    for layer in project_info.layers:
        func_count = sum(len(c.functions) for c in layer.components)
        class_count = sum(len(c.classes) for c in layer.components)
        logger.info(f"    {layer.name}: {func_count} 함수, {class_count} 클래스")

    return 0


if __name__ == "__main__":
    sys.exit(main())
