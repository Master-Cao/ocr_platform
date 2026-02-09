# ocrdetect 库 API 文档

中文 OCR 检测与识别库，基于 chineseocr-lite + ONNX Runtime。提供 C 与 C++ 接口，便于集成到各类应用。

---

## 如何获取识别出的具体文字

- **C 接口**：`ocr_detect` 的返回值是**文本框个数** `n`；**具体识别文字**在输出数组里：`results[0].text`～`results[n-1].text`，每个元素对应一块文字的识别结果。使用完后对每个非 NULL 的 `results[i].text` 调用 `ocr_free_text(results[i].text)` 释放。
- **C++ 接口**：`detect()` 返回 `std::vector<TextBlock>`，直接取 `blocks[i].text` 即为第 i 个文本框的识别文字，无需释放。

---

## 头文件

| 头文件 | 说明 |
|--------|------|
| `ocrdetect/ocr_api.h` | C 接口 |
| `ocrdetect/OcrEngine.hpp` | C++ 封装（依赖 OpenCV `cv::Mat`） |

---

## 一、C API（ocr_api.h）

### 1.1 类型定义

#### OCR_Handle

```c
typedef void* OCR_Handle;
```

引擎句柄，由 `ocr_create` 返回，传入其他接口使用。不透明，不可解引用。

---

#### OCR_Point

```c
typedef struct OCR_Point {
  double x, y;
} OCR_Point;
```

二维点坐标，用于框顶点。

---

#### OCR_Options

```c
typedef struct OCR_Options {
  int padding;           /* 外扩白边，默认 0 */
  int short_side_len;    /* 短边缩放目标，0 表示不缩放，默认 960 */
  float box_score_thresh;
  float box_thresh;
  float un_clip_ratio;
  int do_angle;          /* 1 启用方向分类，0 不启用 */
  int most_angle;        /* 1 使用角度投票 */
} OCR_Options;
```

检测时的可调参数。调用 `ocr_detect` 时传 `NULL` 则使用内部默认值。

---

#### OCR_TextBlock

```c
typedef struct OCR_TextBlock {
  OCR_Point box[4];   /* 四个顶点：左上、右上、右下、左下 */
  float box_score;
  char* text;         /* 该文本框识别出的具体文字（由库 malloc，需用 ocr_free_text 释放） */
  float confidence;
} OCR_TextBlock;
```

单行/单块文本的检测与识别结果。**`text` 即为该块识别出的具体文字内容**（如 "你好"、"123"）；`box[4]` 为四边形四个顶点。调用方必须对每个非 NULL 的 `text` 调用 `ocr_free_text(block.text)` 释放。

---

### 1.2 函数说明

#### ocr_create

```c
OCR_Handle ocr_create(const char* models_dir);
```

- **功能**：创建 OCR 引擎并在创建时加载模型。
- **参数**：`models_dir` 为模型目录路径（UTF-8），目录内需包含：
  - `det.onnx`
  - `cls.onnx`
  - `rec.onnx`
  - `ppocr_keys_v1.txt` 或 `keys.txt`
- **返回**：成功返回非 NULL 句柄，失败返回 `NULL`。
- **示例**：`OCR_Handle h = ocr_create("./OcrDetect/models");`

---

#### ocr_destroy

```c
void ocr_destroy(OCR_Handle h);
```

- **功能**：销毁引擎并释放资源。
- **参数**：`h` 为 `ocr_create` 返回的句柄，可为 `NULL`（无操作）。

---

#### ocr_set_num_threads

```c
void ocr_set_num_threads(OCR_Handle h, int n);
```

- **功能**：设置推理使用的线程数。
- **参数**：`h` 句柄；`n` 线程数（如 4）。

---

#### ocr_detect

```c
int ocr_detect(
  OCR_Handle h,
  const unsigned char* image_data, int width, int height, int channels,
  const OCR_Options* options,
  OCR_TextBlock* results, int max_results);
```

- **功能**：对图像做文本检测与识别，**每个检测到的文本框都会得到一行识别文字**。
- **参数**：
  - `h`：引擎句柄。
  - `image_data`：图像数据，行优先，BGR 或灰度。
  - `width`, `height`, `channels`：宽、高、通道数（3 或 1）。
  - `options`：检测选项，可为 `NULL` 使用默认。
  - `results`：输出数组，由调用方分配，至少 `max_results` 个 `OCR_TextBlock`；**识别出的具体文字会写入 `results[i].text`**。
  - `max_results`：最多返回的文本框数量。
- **返回**：实际检测到的文本框**个数**（≥0）；<0 表示错误。**识别出的文字在 `results[0].text`～`results[n-1].text` 中**，每个元素对应一行/一块文字。
- **注意**：每个 `results[i].text` 由库分配，使用完毕后需对每个非 NULL 的 `results[i].text` 调用 `ocr_free_text(results[i].text)`。

---

#### ocr_free_text

```c
void ocr_free_text(char* text);
```

- **功能**：释放 `ocr_detect` 中由库分配的 `OCR_TextBlock::text`。
- **参数**：`text` 为某次 `ocr_detect` 返回的 `block.text`，可为 `NULL`。

---

### 1.3 C 调用示例

```c
#include <ocrdetect/ocr_api.h>
#include <stdio.h>

int main(void) {
  OCR_Handle h = ocr_create("./OcrDetect/models");
  if (!h) return -1;
  ocr_set_num_threads(h, 4);

  /* 假设 image_data 为 BGR，宽 w 高 h */
  OCR_Options opt = { .padding = 0, .short_side_len = 960, .do_angle = 1 };
  OCR_TextBlock blocks[64];
  int n = ocr_detect(h, image_data, w, h, 3, &opt, blocks, 64);
  for (int i = 0; i < n; i++) {
    printf("%s\n", blocks[i].text);
    ocr_free_text(blocks[i].text);
  }
  ocr_destroy(h);
  return 0;
}
```

---

## 二、C++ API（OcrEngine.hpp）

命名空间：`ocrdetect`。

### 2.1 类型

#### TextBlock

```cpp
struct TextBlock {
  float box[4][2];   // 四个顶点 (x,y)
  float box_score;
  std::string text;  // 该文本框识别出的具体文字
  float confidence;
};
```

单行结果，与 C 的 `OCR_TextBlock` 对应。**`text` 即为该块识别出的具体文字内容**，由类内部管理，无需手动释放。

---

### 2.2 类 OcrEngine

#### 构造

```cpp
explicit OcrEngine(const std::string& models_dir);
```

- **功能**：创建引擎并加载模型。
- **参数**：`models_dir` 为模型目录（同 C 的 `ocr_create`）。
- **说明**：加载失败时内部句柄为 NULL，可通过 `valid()` 检查。

---

#### valid

```cpp
bool valid() const;
```

- **返回**：引擎是否创建并加载成功（句柄非 NULL）。

---

#### setNumThreads

```cpp
void setNumThreads(int n);
```

- **功能**：设置推理线程数。

---

#### detect

```cpp
std::vector<TextBlock> detect(
  const cv::Mat& image,
  int padding = 0,
  int short_side_len = 960,
  float box_score_thresh = 0.6f,
  float box_thresh = 0.3f,
  float un_clip_ratio = 2.0f,
  bool do_angle = true,
  bool most_angle = true);
```

- **功能**：对 `image` 做 OCR，返回文本框列表；**每个元素的 `text` 即为该块识别出的具体文字**。
- **参数**：与 C 的 `OCR_Options` 对应；`image` 为 OpenCV 图像（BGR 或灰度）。
- **返回**：`std::vector<TextBlock>`，`blocks[i].text` 为第 i 个文本框的识别文字，无需手动释放内存。

---

### 2.3 C++ 调用示例

```cpp
#include <ocrdetect/OcrEngine.hpp>
#include <opencv2/opencv.hpp>

int main() {
  ocrdetect::OcrEngine engine("./OcrDetect/models");
  if (!engine.valid()) return -1;
  engine.setNumThreads(4);

  cv::Mat img = cv::imread("test.png");
  auto blocks = engine.detect(img);
  for (const auto& b : blocks)
    std::cout << b.text << " (置信度: " << b.confidence << ")\n";
  return 0;
}
```

---

## 三、模型与依赖

- **模型目录**：需包含 `det.onnx`、`cls.onnx`、`rec.onnx` 以及 `ppocr_keys_v1.txt` 或 `keys.txt`。项目内示例路径：`OcrDetect/models/`。
- **依赖**：OpenCV、ONNX Runtime（库内已包含在 `OcrDetect/onnxruntime-static/`）。
