#!/bin/bash
set -e

echo "Installing grip..."

# 의존성 확인
command -v g++ >/dev/null 2>&1 || { echo "g++ required"; exit 1; }
command -v git >/dev/null 2>&1 || { echo "git required"; exit 1; }

# 임시 디렉토리에서 빌드
TMPDIR=$(mktemp -d)
git clone https://github.com/dvdsvds/grip.git "$TMPDIR/grip"
cd "$TMPDIR/grip"
./build.sh

# 설치
sudo cp grip /usr/local/bin/grip
echo "grip installed to /usr/local/bin/grip"

# 정리
rm -rf "$TMPDIR"

echo "Done! Run 'grip new myproject' to get started."
