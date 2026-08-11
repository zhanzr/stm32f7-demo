# Build the STM32F769NI blink_hello template with CMake + Ninja.
# Run with:  bash build.sh    (or ./build.sh on Linux)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Prefer the MSYS2 mingw64 environment (newer CMake/Ninja) when present.
if [ -d /mingw64/bin ] && ! command -v cmake >/dev/null 2>&1; then
    export PATH="/mingw64/bin:/usr/bin:$PATH"
fi

# Regenerate the bundled site from the standalone e_server/ sources
# (e_server/web + e_server/public -> Inc/web_assets.h).
python "$SCRIPT_DIR/../../../e_server/build_web.py" --out "$SCRIPT_DIR/Inc/web_assets.h"

mkdir -p build
cd build
cmake -G Ninja "$@" ..
ninja
