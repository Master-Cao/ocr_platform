#!/usr/bin/env bash
# 模板匹配模块编译脚本（独立构建，仅依赖 OpenCV）
# 用法: ./build.sh [Debug|Release] [选项]

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"

while [[ $# -gt 0 ]]; do
  case $1 in
    Debug|Release|RelWithDebInfo|MinSizeRel)
      BUILD_TYPE="$1"
      shift
      ;;
    --clean)
      if [[ -d "$BUILD_DIR" ]]; then
        echo "删除已存在的 $BUILD_DIR/"
        rm -rf "$BUILD_DIR"
      fi
      shift
      ;;
    -h|--help)
      echo "用法: $0 [Debug|Release] [选项]"
      echo "  独立编译模板匹配库和 demo_tm，仅需 OpenCV"
      echo "  --clean    先删除 build 再配置"
      exit 0
      ;;
    *)
      shift
      ;;
  esac
done

echo "========== 模板匹配 编译 =========="
echo "项目目录: $SCRIPT_DIR"
echo "构建类型: $BUILD_TYPE  构建目录: $BUILD_DIR"

if ! command -v cmake &>/dev/null; then
  echo "错误: 未找到 cmake"
  exit 1
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      ..
cmake --build . -j"$(nproc 2>/dev/null || echo 4)"

echo ""
echo "========== 编译完成 =========="
echo "可执行文件: $SCRIPT_DIR/$BUILD_DIR/bin/demo_tm"
echo ""
echo "运行示例:"
echo "  $SCRIPT_DIR/$BUILD_DIR/bin/demo_tm <场景图> <模板图> [输出图]"
echo "  ./$BUILD_DIR/bin/demo_tm scene.png template.png result.png"
