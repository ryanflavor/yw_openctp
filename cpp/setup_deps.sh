#!/usr/bin/env bash
# Download CTP 6.7.11 SDK headers + extract TTS .so from openctp-ctp Python package.
# Usage: cd cpp && bash setup_deps.sh
set -euo pipefail

DEPS_DIR="$(cd "$(dirname "$0")" && pwd)/deps"
mkdir -p "$DEPS_DIR/include" "$DEPS_DIR/lib"

# --- 1. Headers: CTP official 6.7.11 ---
SDK_ZIP="/tmp/ctp_6.7.11.zip"
SDK_DIR="/tmp/ctp_6.7.11"
if [ ! -f "$SDK_ZIP" ]; then
    echo "[1/2] Downloading CTP 6.7.11 SDK ..."
    wget -q -O "$SDK_ZIP" 'http://openctp.cn/download/CTPAPI/CTP/ctp_6.7.11.zip'
fi
rm -rf "$SDK_DIR"
unzip -qo "$SDK_ZIP" -d "$SDK_DIR"

HEADER_DIR=$(find "$SDK_DIR" -path '*linux64*' -name 'ThostFtdcTraderApi.h' -printf '%h' -quit)
if [ -z "$HEADER_DIR" ]; then
    echo "ERROR: Cannot find header files in SDK zip" >&2; exit 1
fi
cp "$HEADER_DIR"/ThostFtdc*.h "$DEPS_DIR/include/"
echo "  Headers installed from: $HEADER_DIR"

# --- 2. TTS .so: extract from openctp-ctp Python package ---
# The openctp-ctp pip package bundles TTS-compatible .so files.
# We need a temporary venv to extract them.
TMP_VENV="/tmp/_ctp_extract_venv"
if [ ! -d "$TMP_VENV" ]; then
    echo "[2/2] Installing openctp-ctp to extract TTS .so ..."
    python3 -m venv "$TMP_VENV"
    "$TMP_VENV/bin/pip" install -q openctp-ctp==6.7.11.0
fi

LIBS_DIR=$(find "$TMP_VENV" -path '*/openctp_ctp.libs' -type d -print -quit)
if [ -z "$LIBS_DIR" ]; then
    echo "ERROR: Cannot find openctp_ctp.libs in venv" >&2; exit 1
fi

# Copy with standard names (strip hash suffix)
for f in "$LIBS_DIR"/libthosttraderapi_se*.so; do
    cp "$f" "$DEPS_DIR/lib/libthosttraderapi_se.so"
    break
done
for f in "$LIBS_DIR"/libthostmduserapi_se*.so; do
    cp "$f" "$DEPS_DIR/lib/libthostmduserapi_se.so"
    break
done

echo "  Libraries installed from: $LIBS_DIR"
echo ""
echo "Done. deps/ contents:"
ls -la "$DEPS_DIR/include/" "$DEPS_DIR/lib/"
