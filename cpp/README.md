# QT_OCR_Service C++（OCR + 模板匹配）

本目录为 C++ 版 OCR 与模板匹配服务，包含：

- **ocrdetect**：OCR 检测与识别库（det/cls/rec.onnx + ppocr_keys_v1.txt）
- **templatematch**：模板匹配库
- **examples**：可执行示例（test_ocr、demo、test_rec）

## 依赖

- CMake 3.14+
- OpenCV
- ONNX Runtime（见下方）

## ONNX Runtime

- **Windows**：将 `onnxruntime-win-x64-*.zip` 解压到 `OcrDetect/` 目录下。
- **Linux**：在项目根目录执行 `./build.sh`，脚本会根据架构自动解压 `onnxruntime-linux-x64-*.tgz` 或 `onnxruntime-linux-aarch64-*.tgz`。

## 构建

```bash
# Linux
./build.sh          # Release
./build.sh Debug    # Debug
./build.sh --clean  # 清理后重新配置

# 或手动
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

可执行文件与库在 `build/bin/`、`build/lib/`。

## 运行示例

模型目录需包含：`det.onnx`、`cls.onnx`、`rec.onnx`、`ppocr_keys_v1.txt`（或 `keys.txt`）。

```bash
cd build/bin
# Linux 需设置库路径
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../lib

# 纯 OCR 测试（对应 Python test_ocr）
./test_ocr <模型目录> <图片路径>
# 例: ./test_ocr ../../OcrDetect/models test.png

# 模板匹配 + OCR 联合示例（两参：仅 OCR；三参：匹配 + 组合 OCR）
./demo [模型目录] [待检测图片] [模板图片]
# 例: ./demo ../../OcrDetect/models test.png
# 例: ./demo ../../OcrDetect/models scene.png template.png

# 仅识别（rec.onnx 单图测试，在 OcrDetect 目录内构建）
./test_rec <裁剪图路径>
```

## 目录结构

```
cpp/
├── CMakeLists.txt
├── build.sh           # Linux 构建脚本
├── common/            # 公共头文件
├── OcrDetect/         # OCR 库（include、src、models、onnxruntime）
├── templatematch/     # 模板匹配库
├── examples/         # test_ocr.cpp、demo.cpp
└── README.md
```
