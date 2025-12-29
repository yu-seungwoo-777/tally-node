#!/bin/bash
# ESP32 컴포넌트로 임베디드 파일 복사

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WEB_DIR="$(dirname "$SCRIPT_DIR")"
EMBED_SRC="$WEB_DIR/dist/embed"
COPY_DEST="$WEB_DIR/../components/02_presentation/web_server/static_embed"

echo ""
echo "📋 Copying embedded files to ESP32 component..."
echo "  Source: $EMBED_SRC"
echo "  Dest:   $COPY_DEST"
echo ""

# 대상 폴더 생성
mkdir -p "$COPY_DEST"

# 파일 복사
if [ -d "$EMBED_SRC" ]; then
    cp -r "$EMBED_SRC"/* "$COPY_DEST/"
    echo "✅ Files copied successfully!"
    echo ""
    echo "Copied files:"
    ls -la "$COPY_DEST"
else
    echo "❌ Error: embed directory not found"
    echo "   Run 'npm run build && npm run embed' first"
    exit 1
fi

echo ""
