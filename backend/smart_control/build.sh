#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

APP_NAME="elnk-console"
GO_DOCKER_IMAGE="${GO_DOCKER_IMAGE:-golang:latest}"

build_one() {
  local target="$1"
  local out_dir="build/${target}"

  rm -rf "$out_dir"
  mkdir -p "$out_dir" dist

  case "$target" in
    darwin-arm64)
      CGO_ENABLED=1 GOOS=darwin GOARCH=arm64 \
        go build -trimpath -ldflags="-s -w" -o "${out_dir}/${APP_NAME}" .
      ;;

    linux-amd64)
      if command -v x86_64-linux-gnu-gcc >/dev/null 2>&1; then
        CC=x86_64-linux-gnu-gcc CGO_ENABLED=1 GOOS=linux GOARCH=amd64 \
          go build -trimpath -ldflags="-s -w" -o "${out_dir}/${APP_NAME}" .
      elif command -v docker >/dev/null 2>&1; then
        docker run --rm \
          --platform linux/amd64 \
          -e PATH="/usr/local/go/bin:/go/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
          -v "$ROOT:/src" \
          -w /src \
          "$GO_DOCKER_IMAGE" \
          bash -lc "GO_BIN=\$(command -v go || true); if [ -z \"\$GO_BIN\" ] && [ -x /usr/local/go/bin/go ]; then GO_BIN=/usr/local/go/bin/go; fi; if [ -z \"\$GO_BIN\" ]; then echo 'Docker 镜像里找不到 go'; echo \"PATH=\$PATH\"; ls -la /usr/local/go/bin 2>/dev/null || true; exit 127; fi; \"\$GO_BIN\" version; CGO_ENABLED=1 GOOS=linux GOARCH=amd64 \"\$GO_BIN\" build -trimpath -ldflags='-s -w' -o '${out_dir}/${APP_NAME}' ."
      else
        echo "无法构建 linux-amd64：项目使用 go-sqlite3，需要 CGO。"
        echo "请安装 x86_64-linux-gnu-gcc，或安装 Docker 后重试。"
        echo "Docker 方式会使用镜像：${GO_DOCKER_IMAGE}"
        exit 1
      fi
      ;;

    *)
      echo "未知目标: $target"
      echo "用法: ./build.sh [all|darwin-arm64|linux-amd64]"
      exit 1
      ;;
  esac

  cp -R static "$out_dir/static"
  tar -C "$out_dir" -czf "dist/elnk-console-${target}.tar.gz" "$APP_NAME" static
  echo "已生成 dist/elnk-console-${target}.tar.gz"
}

target="${1:-all}"
case "$target" in
  all)
    build_one darwin-arm64
    build_one linux-amd64
    ;;
  darwin-arm64|linux-amd64)
    build_one "$target"
    ;;
  *)
    echo "用法: ./build.sh [all|darwin-arm64|linux-amd64]"
    exit 1
    ;;
esac
