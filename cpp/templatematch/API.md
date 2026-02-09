# templatematch 库 API 文档

图像模板匹配库，基于 Fastest Image Pattern Matching。提供 C 与 C++ 接口，用于在场景图中查找与模板图一致或相似的位置与角度。

---

## 头文件

| 头文件 | 说明 |
|--------|------|
| `templatematch/tm_api.h` | C 接口 |
| `templatematch/Matcher.hpp` | C++ 封装（依赖 OpenCV `cv::Mat`） |

---

## 一、C API（tm_api.h）

### 1.1 类型定义

#### TM_Handle

```c
typedef void* TM_Handle;
```

匹配器句柄，由 `tm_create` 返回。不透明，不可解引用。

---

#### TM_Params

```c
typedef struct TM_Params {
  int max_count;           /* 最大匹配数量，默认 200 */
  double score_threshold;  /* 得分阈值 [0,1]，默认 0.5 */
  double iou_threshold;    /* 重叠框 IOU 去重阈值，默认 0.0 */
  double angle;            /* 匹配角度范围，默认 0 */
  double min_area;         /* 顶层金字塔最小面积，默认 256 */
} TM_Params;
```

创建匹配器时的参数。`tm_create` 传入 `NULL` 时使用上述默认值。

---

#### TM_MatchResult

```c
typedef struct TM_MatchResult {
  double left_top_x, left_top_y;
  double left_bottom_x, left_bottom_y;
  double right_top_x, right_top_y;
  double right_bottom_x, right_bottom_y;
  double center_x, center_y;
  double angle;
  double score;
} TM_MatchResult;
```

单次匹配结果：模板在场景中的四角、中心、旋转角度和匹配得分。

---

### 1.2 函数说明

#### tm_create

```c
TM_Handle tm_create(const TM_Params* params);
```

- **功能**：创建模板匹配器。
- **参数**：`params` 为匹配参数，传 `NULL` 则使用默认参数。
- **返回**：成功返回非 NULL 句柄，失败返回 `NULL`。
- **示例**：`TM_Handle h = tm_create(NULL);` 或先填充 `TM_Params` 再传入。

---

#### tm_destroy

```c
void tm_destroy(TM_Handle h);
```

- **功能**：销毁匹配器并释放资源。
- **参数**：`h` 为 `tm_create` 返回的句柄，可为 `NULL`（无操作）。

---

#### tm_set_template_from_memory

```c
int tm_set_template_from_memory(
  TM_Handle h,
  const unsigned char* data, int width, int height, int channels);
```

- **功能**：从内存设置模板图像（灰度）。
- **参数**：
  - `h`：句柄。
  - `data`：图像数据，行优先。
  - `width`, `height`, `channels`：宽、高、通道数；**仅支持 channels == 1**（灰度）。
- **返回**：0 成功，<0 失败。

---

#### tm_set_template_from_file

```c
int tm_set_template_from_file(TM_Handle h, const char* file_path);
```

- **功能**：从文件加载模板图像（内部会转为灰度）。
- **参数**：`h` 句柄；`file_path` 文件路径（UTF-8）。
- **返回**：0 成功，<0 失败。
- **示例**：`tm_set_template_from_file(h, "template.png");`

---

#### tm_match

```c
int tm_match(
  TM_Handle h,
  const unsigned char* image_data, int width, int height, int channels,
  TM_MatchResult* results, int max_results);
```

- **功能**：在场景图中进行模板匹配。
- **参数**：
  - `h`：句柄。
  - `image_data`：场景图数据，行优先，**灰度**（channels == 1）。
  - `width`, `height`, `channels`：场景图宽、高、通道数（仅支持 1）。
  - `results`：输出数组，由调用方分配，至少 `max_results` 个 `TM_MatchResult`。
  - `max_results`：最多返回的匹配数量。
- **返回**：实际匹配数量（≥0）；<0 表示错误。
- **说明**：结果按匹配质量排序，`results[0]` 为最佳匹配。

---

### 1.3 C 调用示例

```c
#include <templatematch/tm_api.h>
#include <stdio.h>

int main(void) {
  TM_Params params = { .max_count = 200, .score_threshold = 0.5, .angle = 0, .min_area = 256 };
  TM_Handle h = tm_create(&params);
  if (!h) return -1;

  if (tm_set_template_from_file(h, "template.png") != 0) {
    tm_destroy(h);
    return -1;
  }

  TM_MatchResult results[32];
  int n = tm_match(h, gray_data, scene_width, scene_height, 1, results, 32);
  for (int i = 0; i < n; i++)
    printf("匹配 %d: 中心(%.1f, %.1f) 角度=%.2f 得分=%.4f\n",
           i + 1, results[i].center_x, results[i].center_y,
           results[i].angle, results[i].score);
  tm_destroy(h);
  return 0;
}
```

---

## 二、C++ API（Matcher.hpp）

命名空间：`templatematch`。

### 2.1 类型

#### Params

```cpp
struct Params {
  int max_count = 200;
  double score_threshold = 0.5;
  double iou_threshold = 0.0;
  double angle = 0.0;
  double min_area = 256.0;
};
```

匹配参数，与 C 的 `TM_Params` 对应。

---

#### MatchResult

```cpp
struct MatchResult {
  double left_top_x, left_top_y;
  double left_bottom_x, left_bottom_y;
  double right_top_x, right_top_y;
  double right_bottom_x, right_bottom_y;
  double center_x, center_y;
  double angle;
  double score;
};
```

单次匹配结果，与 C 的 `TM_MatchResult` 对应。

---

### 2.2 类 Matcher

#### 构造

```cpp
explicit Matcher(const Params& params = Params());
```

- **功能**：创建匹配器。
- **参数**：`params` 可选，不传则使用默认 `Params()`。

---

#### valid

```cpp
bool valid() const;
```

- **返回**：匹配器是否创建成功（内部句柄非 NULL）。

---

#### setTemplate

```cpp
bool setTemplate(const cv::Mat& templateImage);
```

- **功能**：从 OpenCV 图像设置模板；若为多通道则内部转灰度。
- **参数**：`templateImage` 模板图（BGR 或灰度）。
- **返回**：成功返回 `true`，失败返回 `false`。

---

#### setTemplateFromFile

```cpp
bool setTemplateFromFile(const std::string& path);
```

- **功能**：从文件加载并设置模板（内部转灰度）。
- **参数**：`path` 文件路径。
- **返回**：成功返回 `true`。

---

#### match

```cpp
std::vector<MatchResult> match(const cv::Mat& image);
```

- **功能**：在 `image` 中做模板匹配。
- **参数**：`image` 场景图（BGR 或灰度，内部会转灰度）。
- **返回**：匹配结果列表，按得分排序；无匹配或出错时返回空 `vector`。

---

### 2.3 C++ 调用示例

```cpp
#include <templatematch/Matcher.hpp>
#include <opencv2/opencv.hpp>

int main() {
  templatematch::Matcher matcher(templatematch::Params{});
  if (!matcher.setTemplateFromFile("template.png")) return -1;

  cv::Mat scene = cv::imread("scene.png");
  auto results = matcher.match(scene);
  for (size_t i = 0; i < results.size(); i++) {
    const auto& r = results[i];
    std::cout << "匹配 " << (i + 1) << ": 中心(" << r.center_x << ", " << r.center_y
              << ") 角度=" << r.angle << " 得分=" << r.score << "\n";
  }
  return 0;
}
```

---

## 三、使用说明

- **图像格式**：C 接口要求模板与场景均为**灰度**（channels=1）；C++ 接口可传入 BGR，内部会转灰度。
- **坐标与角度**：结果中的坐标为在场景图中的像素位置；`angle` 为模板相对场景的旋转角度（度）。
- **依赖**：仅依赖 OpenCV，无需 ONNX Runtime。
