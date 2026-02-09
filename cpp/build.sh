#!/usr/bin/env bash
# QT_OCR_Service C++ 编译脚本（OCR + 模板匹配）
# 用法: ./build.sh [Debug|Release] [选项]

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"
INSTALL_PREFIX=""

while [[ $# -gt 0 ]]; do
  case $1 in
    Debug|Release|RelWithDebInfo|MinSizeRel)
      BUILD_TYPE="$1"
      shift
      ;;
    --install-prefix)
      INSTALL_PREFIX="$2"
      shift 2
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
      echo "  --install-prefix <dir>  安装目录（可选）"
      echo "  --clean          先删除 build 再配置"
      exit 0
      ;;
    *)
      shift
      ;;
  esac
done

echo "========== QT_OCR_Service C++ 编译 =========="
echo "构建类型: $BUILD_TYPE  构建目录: $BUILD_DIR"

if ! command -v cmake &>/dev/null; then
  echo "错误: 未找到 cmake"
  exit 1
fi

# 确保 OCR 结果图片输出目录存在并具有写权限
IMG_DIR="/var/ocr/images"
echo "检查图片输出目录: $IMG_DIR"
if [[ ! -d "$IMG_DIR" ]]; then
  echo "创建目录: $IMG_DIR (可能需要 sudo)"
  if ! sudo mkdir -p "$IMG_DIR"; then
    echo "错误: 无法创建目录 $IMG_DIR，请手动创建并赋予写权限。"
    exit 1
  fi
fi

# 直接赋予 777 权限，确保可写（注意仅适用于测试/开发环境）
echo "设置 $IMG_DIR 权限为 777 (可能需要 sudo)"
if ! sudo chmod 777 "$IMG_DIR"; then
  echo "警告: 无法修改 $IMG_DIR 权限，后续保存结果图片可能失败。"
fi

ORT_DIR="$SCRIPT_DIR/OcrDetect"
HOST_ARCH="$(uname -m)"
case "$HOST_ARCH" in
  x86_64|x64)   ORT_ARCH="x64" ;;
  aarch64|arm64) ORT_ARCH="aarch64" ;;
  *) ORT_ARCH="$HOST_ARCH" ;;
esac

ORT_TGZ="$(ls "$ORT_DIR"/onnxruntime-linux-${ORT_ARCH}-*.tgz 2>/dev/null | head -1)"
if [[ -n "$ORT_TGZ" ]]; then
  ORT_BASENAME="$(basename "$ORT_TGZ" .tgz)"
  ORT_EXTRACT_DIR="$ORT_DIR/$ORT_BASENAME"
  if [[ ! -f "$ORT_EXTRACT_DIR/lib/libonnxruntime.so" ]] && ! ls "$ORT_EXTRACT_DIR"/lib/libonnxruntime.so* &>/dev/null 2>&1; then
    echo "[ONNX Runtime] 解压 $ORT_TGZ ..."
    tar -xzf "$ORT_TGZ" -C "$ORT_DIR"
  fi
  ORT_NESTED="$ORT_EXTRACT_DIR/include/onnxruntime/core/session"
  if [[ ! -d "$ORT_NESTED" ]]; then
    mkdir -p "$ORT_NESTED"
    for hdr in "$ORT_EXTRACT_DIR"/include/*.h; do
      ln -sf "$hdr" "$ORT_NESTED/$(basename "$hdr")"
    done
  fi
else
  echo "错误: 未找到 onnxruntime-linux-${ORT_ARCH}-*.tgz 于 $ORT_DIR"
  exit 1
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
CMAKE_ARGS=(-DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)
[[ -n "$INSTALL_PREFIX" ]] && CMAKE_ARGS+=(-DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX")
cmake "${CMAKE_ARGS[@]}" ..
cmake --build . -j"$(nproc)"

# 注册库目录到系统库搜索路径，避免每次手动设置 LD_LIBRARY_PATH
LIB_DIR="$SCRIPT_DIR/$BUILD_DIR/lib"
if [[ -d "$LIB_DIR" ]]; then
  echo "注册运行时库路径: $LIB_DIR (需要 sudo)"
  CONF_FILE="/etc/ld.so.conf.d/qt_ocr_service.conf"
  echo "$LIB_DIR" | sudo tee "$CONF_FILE" >/dev/null || {
    echo "警告: 无法写入 $CONF_FILE，请检查 sudo 权限。"
  }
  if command -v ldconfig &>/dev/null; then
    sudo ldconfig || echo "警告: 执行 ldconfig 失败，可能仍需手动设置 LD_LIBRARY_PATH。"
  fi
else
  echo "警告: 未找到库目录 $LIB_DIR，无法注册到系统库路径。"
fi

# 将 test_ocr 注册为系统命令，方便在任意目录直接调用
BIN_SRC="$SCRIPT_DIR/$BUILD_DIR/bin/test_ocr"
if [[ -f "$BIN_SRC" ]]; then
  TARGET_BIN="/usr/local/bin/test_ocr"
  echo "注册系统命令: test_ocr -> $BIN_SRC (需要 sudo)"
  if ! sudo ln -sf "$BIN_SRC" "$TARGET_BIN"; then
    echo "警告: 无法创建/更新 $TARGET_BIN，请检查 sudo 权限。"
  fi
else
  echo "警告: 未找到可执行文件 $BIN_SRC，跳过注册 test_ocr 命令。"
fi

echo ""
echo "========== 编译完成 =========="
echo "可执行文件: $BUILD_DIR/bin/"
echo "运行示例 (任意目录):"
echo "  test_ocr <模型目录> <图片路径>"
echo "demo 示例仍位于: $BUILD_DIR/bin/demo"
[[ -n "$INSTALL_PREFIX" ]] && cmake --install .
