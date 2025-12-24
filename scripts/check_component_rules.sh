#!/bin/bash
# 컴포넌트 구조 규칙 검사 스크립트

echo "=== 컴포넌트 구조 규칙 검사 ==="
echo ""

COMPONENT_DIR="components"
ARCH_DOC="docs/ARCHITECTURE.md"
ERRORS=0
WARNINGS=0

# ============================================================================
# [1] src/ 폴더 사용 검사 (핵심 규칙)
# ============================================================================
echo "[1] src/ 폴더 사용 검사..."
SRC_FOLDERS=$(find "$COMPONENT_DIR" -type d -name "src" 2>/dev/null)
if [ -n "$SRC_FOLDERS" ]; then
    echo "❌ 발견: src/ 폴더를 사용하는 컴포넌트"
    echo "$SRC_FOLDERS"
    ERRORS=$((ERRORS + 1))
else
    echo "✅ src/ 폴더 없음"
fi
echo ""

# ============================================================================
# [2] CMakeLists.txt의 src/ 경로 사용 검사
# ============================================================================
echo "[2] CMakeLists.txt의 src/ 경로 검사..."
BAD_CMAKES=$(grep -r "SRCS.*src/" "$COMPONENT_DIR"/*/CMakeLists.txt 2>/dev/null)
if [ -n "$BAD_CMAKES" ]; then
    echo "❌ 발견: src/ 경로를 참조하는 CMakeLists.txt"
    echo "$BAD_CMAKES"
    ERRORS=$((ERRORS + 1))
else
    echo "✅ src/ 경로 참조 없음"
fi
echo ""

# ============================================================================
# [3] CMakeLists.txt 규칙 검사
# ============================================================================
echo "[3] CMakeLists.txt 규칙 검사..."
echo ""

CMAKE_ERRORS=0
CMAKE_WARNINGS=0

# 레이어 폴더 목록 (실제 컴포넌트가 아님, 건너뜀)
LAYER_FOLDERS="00_common 01_app 02_presentation 03_service 04_driver 05_hal"

# 메타 컴포넌트 목록 (소스가 없는 컴포넌트, 건너뜀)
META_COMPS="components/04_driver/switcher_driver"

# 모든 컴포넌트의 CMakeLists.txt 확인 (레이어 폴더 제외)
for cmake_file in $(find "$COMPONENT_DIR" -name "CMakeLists.txt" -type f); do
    # 레이어 폴더의 CMakeLists.txt는 건너뜀
    IS_LAYER=0
    for layer in $LAYER_FOLDERS; do
        if [ "$(dirname "$cmake_file")" = "$COMPONENT_DIR/$layer" ]; then
            IS_LAYER=1
            break
        fi
    done
    if [ $IS_LAYER -eq 1 ]; then
        continue
    fi

    # 메타 컴포넌트는 건너뜀
    IS_META=0
    for meta in $META_COMPS; do
        if [ "$(dirname "$cmake_file")" = "$meta" ]; then
            IS_META=1
            break
        fi
    done
    if [ $IS_META -eq 1 ]; then
        continue
    fi

    COMP_DIR=$(dirname "$cmake_file")

    # 3.1 idf_component_register 사용 확인
    if ! grep -q "idf_component_register" "$cmake_file"; then
        echo "⚠️  $cmake_file: idf_component_register 없음"
        CMAKE_WARNINGS=$((CMAKE_WARNINGS + 1))
        continue
    fi

    # 3.2 SRCS에서 src/ 경로 사용 확인 (금지)
    if grep -E "SRCS.*src/" "$cmake_file" > /dev/null 2>&1; then
        echo "❌ $cmake_file: SRCS에 src/ 경로 사용 (금지)"
        CMAKE_ERRORS=$((CMAKE_ERRORS + 1))
    fi

    # 3.3 INCLUDE_DIRS "include" 사용 확인
    #    멀티라인 INCLUDE_DIRS를 처리하기 위해 구간을 추출하여 검사
    INCLUDE_SECTION=$(sed -n '/INCLUDE_DIRS/,/REQUIRES\|PUBLIC\|PRIVATE/p' "$cmake_file" 2>/dev/null || true)
    if ! echo "$INCLUDE_SECTION" | grep -q '"include"' && ! echo "$INCLUDE_SECTION" | grep -q "'include'"; then
        # include/ 폴더가 실제로 존재하는지도 확인
        if [ -d "$COMP_DIR/include" ]; then
            echo "⚠️  $cmake_file: include/ 폴더가 있으나 INCLUDE_DIRS에 누락"
            CMAKE_WARNINGS=$((CMAKE_WARNINGS + 1))
        fi
    fi

    # 3.4 SRCS에 지정된 파일이 실제로 존재하는지 확인
    # 주석 무시하고 실제 SRCS 라인만 추출
    SRCS_LINE=$(grep -E "^\s*SRCS" "$cmake_file" | grep -v "^[[:space:]]*#" | head -1)
    if [ -n "$SRCS_LINE" ]; then
        # 따옴표로 묶인 파일명 추출
        SRCS=$(echo "$SRCS_LINE" | grep -oE '"[^"]+"' | tr -d '"' || true)
        for src in $SRCS; do
            if [ -n "$src" ] && [ ! -f "$COMP_DIR/$src" ]; then
                echo "❌ $cmake_file: SRCS 파일 없음 - $src"
                CMAKE_ERRORS=$((CMAKE_ERRORS + 1))
            fi
        done
    fi
done

# 3.5 루트 CMakeLists.txt의 5계층 구조 확인
ROOT_CMAKE="CMakeLists.txt"
if [ -f "$ROOT_CMAKE" ]; then
    # 필수 레이어 등록 확인
    for layer in 00_common 01_app 02_presentation 03_service 04_driver 05_hal; do
        if ! grep -q "components/$layer" "$ROOT_CMAKE"; then
            echo "❌ 루트 CMakeLists.txt: $layer 레이어 등록 없음"
            CMAKE_ERRORS=$((CMAKE_ERRORS + 1))
        fi
    done
fi

if [ $CMAKE_ERRORS -eq 0 ] && [ $CMAKE_WARNINGS -eq 0 ]; then
    echo "✅ CMakeLists.txt 규칙 준수"
elif [ $CMAKE_ERRORS -eq 0 ]; then
    echo "⚠️  CMakeLists.txt: $CMAKE_WARNINGS 개 경고"
    WARNINGS=$((WARNINGS + CMAKE_WARNINGS))
else
    echo "❌ CMakeLists.txt: $CMAKE_ERRORS 개 문제"
    ERRORS=$((ERRORS + CMAKE_ERRORS))
fi
echo ""

# ============================================================================
# [4] 감지된 폴더 구조 출력
# ============================================================================
echo "[4] 감지된 컴포넌트 구조..."
echo ""

# 각 계층별 컴포넌트 출력
for layer in 00_common 01_app 02_presentation 03_service 04_driver 05_hal; do
    LAYER_PATH="$COMPONENT_DIR/$layer"
    if [ -d "$LAYER_PATH" ]; then
        echo "[$layer]"

        # 2계층 깊이 컴포넌트 (예: 03_service/button_poll)
        DEPTH2_COMPS=$(find "$LAYER_PATH" -mindepth 2 -maxdepth 2 -type d -name "include" 2>/dev/null | \
            sed "s|$LAYER_PATH/||" | sed 's|/include||' | sort)

        # 3계층 깊이 컴포넌트 (예: 04_driver/switcher_driver/atem)
        DEPTH3_COMPS=$(find "$LAYER_PATH" -mindepth 3 -maxdepth 3 -type d -name "include" 2>/dev/null | \
            sed "s|$LAYER_PATH/||" | sed 's|/include||' | sort)

        # 출력을 위해 그룹핑
        if [ -n "$DEPTH2_COMPS" ] || [ -n "$DEPTH3_COMPS" ]; then
            # 3계층 컴포넌트의 부모 목록 추출
            PARENTS=$(echo "$DEPTH3_COMPS" 2>/dev/null | cut -d'/' -f1 | sort | uniq)

            # 2계층 컴포넌트 출력 (하위 컴포넌트가 있는 것은 제외)
            if [ -n "$DEPTH2_COMPS" ]; then
                echo "$DEPTH2_COMPS" | while read comp; do
                    # 하위에 3계층 컴포넌트가 있는지 체크
                    if echo "$PARENTS" | grep -q "^${comp}$"; then
                        # 하위 컴포넌트가 있음 - 나중에 3계층에서 출력
                        :
                    else
                        echo "  └─ $comp"
                    fi
                done
            fi

            # 3계층 컴포넌트 출력 (부모별로 그룹핑)
            if [ -n "$DEPTH3_COMPS" ]; then
                PREV_PARENT=""
                echo "$DEPTH3_COMPS" | while read comp_path; do
                    PARENT=$(echo "$comp_path" | cut -d'/' -f1)
                    CHILD=$(echo "$comp_path" | cut -d'/' -f2)
                    # 부모가 바뀌었으면 출력
                    if [ "$PREV_PARENT" != "$PARENT" ]; then
                        echo "  └─ $PARENT/"
                        PREV_PARENT="$PARENT"
                    fi
                    echo "    └─ $CHILD"
                done
            fi
        else
            echo "  (비어있음)"
        fi
        echo ""
    fi
done

# ============================================================================
# [5] ARCHITECTURE.md와 실제 컴포넌트 동기화 (Claude CLI 사용)
# ============================================================================
echo "[5] ARCHITECTURE.md와 실제 컴포넌트 동기화 검사..."

if [ ! -f "$ARCH_DOC" ]; then
    echo "⚠️  ARCHITECTURE.md 파일 없음"
    WARNINGS=$((WARNINGS + 1))
elif ! command -v claude &> /dev/null; then
    echo "⚠️  Claude CLI를 찾을 수 없음 (비교 건너뜀)"
    WARNINGS=$((WARNINGS + 1))
else
    # 실제 폴더 구조를 트리 형태로 생성
    REAL_TREE=$(cd "$COMPONENT_DIR" && find . -type d -name "include" | sort | \
        sed 's|^\./||' | sed 's|/include||' | \
        awk '
        {
            n = split($0, parts, "/");
            if (n == 1) {
                layer[parts[1]] = 1;
            } else if (n == 2) {
                parent = parts[1];
                child = parts[2];
                key = parent "|" child;
                if (count[parent] == 0) count[parent] = 0;
                count[parent]++;
                children[parent, count[parent]] = child;
            }
        }
        END {
            layers[1] = "00_common"; layers[2] = "01_app"; layers[3] = "02_presentation";
            layers[4] = "03_service"; layers[5] = "04_driver"; layers[6] = "05_hal";
            for (i = 1; i <= 6; i++) {
                l = layers[i];
                if (layer[l]) {
                    printf "├── %s/\n", l;
                    if (count[l] > 0) {
                        for (j = 1; j <= count[l]; j++) {
                            printf "│   ├── %s/\n", children[l, j];
                        }
                    }
                }
            }
        }')

    # ARCHITECTURE.md 전체 내용 읽기
    DOC_CONTENT=$(cat "$ARCH_DOC")

    # Claude CLI로 검사 및 수정
    CLAUDE_RESULT=$(echo "작업: docs/ARCHITECTURE.md 파일의 '컴포넌트 폴더 구조' 섹션을 실제 components/ 폴더와 동기화

=== 실제 components/ 폴더 구조 ===
$REAL_TREE

=== 현재 문서 내용 ===
$DOC_CONTENT

=== 지시사항 ===
1. '## 컴포넌트 폴더 구조' 섹션과 '## 컴포넌트 상세' 섹션 사이의 트리를 확인
2. 실제 폴더 구조와 비교해서 일치하지 않으면 수정
3. 주의: switcher_driver/ 같은 폴더는 하위에 atem/, obs/, vmix/가 있는 경우 부모 폴더로 표시
4. 수정이 필요하면 전체 파일 내용을 출력 (수정된 버전)
5. 수정이 필요 없으면 'OK'만 출력

파일 내용을 출력할 때는 코드 블록(\`\`\`) 없이 원문 그대로 출력해." | claude 2>&1)

    # 결과 처리
    if echo "$CLAUDE_RESULT" | grep -q "^OK$"; then
        echo "✅ ARCHITECTURE.md와 실제 구조 일치"
    elif echo "$CLAUDE_RESULT" | grep -q "^# 아키텍처"; then
        # Claude가 수정된 파일을 반환한 경우
        echo "📝 문서를 자동 수정합니다..."
        echo "$CLAUDE_RESULT" > "$ARCH_DOC"
        echo "✅ ARCHITECTURE.md가 동기화되었습니다"
    else
        echo "⚠️  Claude CLI 분석 실패"
        echo "$CLAUDE_RESULT" | head -3
        WARNINGS=$((WARNINGS + 1))
    fi
fi
echo ""

# ============================================================================
# 결과 요약
# ============================================================================
echo "===================="
if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo "✅ 모든 규칙 통과"
    exit 0
elif [ $ERRORS -eq 0 ]; then
    echo "⚠️  $WARNINGS 개의 경고 있음"
    exit 0
else
    echo "❌ $ERRORS 개의 문제 발견"
    exit 1
fi
