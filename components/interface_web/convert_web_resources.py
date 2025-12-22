#!/usr/bin/env python3
"""
웹 리소스 파일을 C 문자열로 변환
CMake 빌드 시스템에서 자동 호출됨
"""

import os
import sys

def file_to_c_string(filepath, varname):
    """파일을 C 문자열 리터럴로 변환"""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # C 문자열로 변환 (이스케이프 처리)
    lines = content.split('\n')
    c_lines = []
    for line in lines:
        # 백슬래시와 따옴표 이스케이프
        escaped = line.replace('\\', '\\\\').replace('"', '\\"')
        c_lines.append(f'"{escaped}\\n"')

    return f'''const char {varname}[] =
{chr(10).join(c_lines)};

const size_t {varname}_len = sizeof({varname}) - 1;
'''

def main():
    if len(sys.argv) < 3:
        print("Usage: convert_web_resources.py <source_dir> <output_file>")
        print("  source_dir: www 디렉토리 경로")
        print("  output_file: 출력할 web_resources.c 파일 경로")
        sys.exit(1)

    source_dir = sys.argv[1]
    output_file = sys.argv[2]

    # 변환할 파일 목록
    files = [
        ('index.html', 'index_html'),
        ('style.css', 'style_css'),
        ('app.js', 'app_js'),
    ]

    output = '''/**
 * web_resources.c
 *
 * 웹 리소스를 C 문자열로 임베드 (자동 생성)
 * 수정하지 마세요! www/ 디렉토리의 원본 파일을 수정하세요.
 */

#include <stdint.h>
#include <stddef.h>

'''

    for filename, varname in files:
        filepath = os.path.join(source_dir, filename)
        if os.path.exists(filepath):
            output += f'/* {filename} */\n'
            output += file_to_c_string(filepath, varname)
            output += '\n'
        else:
            print(f'Warning: {filepath} not found')
            sys.exit(1)

    # 출력 파일 저장
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(output)

    print(f'Generated: {output_file}')

if __name__ == '__main__':
    main()
