# QT_OCR_Service 统一 API 协议

本文档描述 C++ 服务端与 Python 服务端共同遵循的请求/响应格式，供 UI（Avalonia）、Web 客户端调用。部署时二选一运行 **cpp/server** 或 **python/server**，客户端通过相同协议访问。

---

## 1. 传输与入口

- **HTTP**：`POST /api`，Content-Type: `application/json`，Body 为 JSON 请求体。
- **MQTT / ZeroMQ**：消息体为同一 JSON 请求格式；响应通过各传输约定回传（如 reply topic、REP 回包）。

---

## 2. 请求格式（Request）

所有请求均为单个 JSON 对象，包含指令类型与参数。

```json
{
  "instruction": "ocr_only",
  "params": { ... },
  "id": 1
}
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `instruction` | string | 是 | 见下表 |
| `params` | object | 是 | 与指令对应的参数，见下文 |
| `id` | number / string | 否 | 请求 id，响应中原样带回，便于对号 |

### 2.1 指令类型（instruction）

| 值 | 说明 |
|----|------|
| `tm_only` | 仅模板匹配：在场景图中查找模板，返回匹配框列表 |
| `ocr_only` | 仅 OCR：对输入图像做文字检测+识别，返回文本框与文本 |
| `tm_then_ocr` | 先模板匹配，再对每个匹配区域做 OCR：需提供场景图与模板图 |

---

## 3. 各指令的 params 与响应

### 3.1 tm_only（仅模板匹配）

**请求 params：**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `scene_image` | string | 是 | 场景图，Base64 编码（支持 JPEG/PNG，带或不带 data URL 前缀均可） |
| `template_image` | string | 是 | 模板图，Base64 编码（若服务端已通过其他方式设置模板则可省略，见实现） |
| `tm_params` | object | 否 | 可选覆盖：max_count, score_threshold, iou_threshold, angle, min_area, top_angle_step |

**成功响应 result：**

```json
{
  "instruction": "tm_only",
  "matches": [
    {
      "left_top": [ 10.5, 20.3 ],
      "right_top": [ 100.2, 20.1 ],
      "right_bottom": [ 100.0, 80.5 ],
      "left_bottom": [ 10.2, 80.4 ],
      "center": [ 55.1, 50.2 ],
      "angle": 0.5,
      "score": 0.92
    }
  ],
  "count": 1
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `matches` | array | 匹配结果列表 |
| `matches[].left_top` | [x, y] | 左上角 |
| `matches[].right_top` | [x, y] | 右上角 |
| `matches[].right_bottom` | [x, y] | 右下角 |
| `matches[].left_bottom` | [x, y] | 左下角 |
| `matches[].center` | [x, y] | 中心点 |
| `matches[].angle` | number | 角度（度） |
| `matches[].score` | number | 得分 [0, 1] |
| `count` | number | 匹配数量 |

---

### 3.2 ocr_only（仅 OCR）

**请求 params：**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `image` | string | 是 | 待识别图像，Base64 编码（JPEG/PNG） |
| `ocr_options` | object | 否 | 可选：padding, short_side_len, box_score_thresh, box_thresh, un_clip_ratio, do_angle, most_angle |

**成功响应 result：**

```json
{
  "instruction": "ocr_only",
  "blocks": [
    {
      "box": [
        [ 10, 20 ],
        [ 100, 20 ],
        [ 100, 40 ],
        [ 10, 40 ]
      ],
      "text": "识别文本",
      "confidence": 0.95,
      "box_score": 0.88
    }
  ],
  "count": 1,
  "detect_time_ms": 45.2
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `blocks` | array | 文本框列表，顶点顺序：左上、右上、右下、左下 |
| `blocks[].box` | array of [x,y] | 四个顶点坐标 |
| `blocks[].text` | string | 识别文本 |
| `blocks[].confidence` | number | 识别置信度 |
| `blocks[].box_score` | number | 检测框得分 |
| `count` | number | 框数量 |
| `detect_time_ms` | number | 可选，检测耗时（毫秒） |

---

### 3.3 tm_then_ocr（先模板匹配再 OCR）

**请求 params：**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `scene_image` | string | 是 | 场景图，Base64 |
| `template_image` | string | 是 | 模板图，Base64 |
| `tm_params` | object | 否 | 同 tm_only |
| `ocr_options` | object | 否 | 同 ocr_only，用于每个匹配区域 |

**成功响应 result：**

```json
{
  "instruction": "tm_then_ocr",
  "regions": [
    {
      "match": {
        "left_top": [ 10, 20 ],
        "right_top": [ 100, 20 ],
        "right_bottom": [ 100, 80 ],
        "left_bottom": [ 10, 80 ],
        "center": [ 55, 50 ],
        "angle": 0,
        "score": 0.92
      },
      "ocr": {
        "blocks": [
          {
            "box": [ [ 0, 0 ], [ 50, 0 ], [ 50, 20 ], [ 0, 20 ] ],
            "text": "区域文字",
            "confidence": 0.9,
            "box_score": 0.85
          }
        ],
        "count": 1
      }
    }
  ],
  "match_count": 1
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `regions` | array | 每个匹配区域对应一项 |
| `regions[].match` | object | 与 tm_only 单条 match 结构相同 |
| `regions[].ocr` | object | 该区域内的 OCR 结果，与 ocr_only 的 blocks/count 一致 |
| `match_count` | number | 匹配到的区域数 |

---

## 4. 统一响应封装（HTTP Body / MQTT Payload）

### 4.1 成功

```json
{
  "jsonrpc": "2.0",
  "result": { ... },
  "id": 1
}
```

`result` 为上述各指令对应的 result 对象（含 instruction、matches/blocks/regions 等）。

### 4.2 错误

```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32602,
    "message": "Invalid params: image required"
  },
  "id": 1
}
```

### 4.3 错误码（error.code）

| 码 | 含义 |
|----|------|
| -32700 | Parse error（请求 JSON 无效） |
| -32600 | Invalid Request（缺少 method/instruction 或格式错误） |
| -32602 | Invalid params（参数缺失、类型错误、image 解码失败等） |
| -32603 | Internal error（引擎异常、模型未加载等） |
| -32000 | 业务错误（如模板设置失败、匹配/OCR 执行失败，message 中可带详情） |

---

## 5. 服务端行为约定

- **初始化**：启动时加载模板匹配模型与 OCR 模型，并执行一次预热（dummy 推理），再开始监听。
- **指令与实现**：
  - `tm_only`：使用已加载的 TM 引擎；若请求中带 `template_image`，可先设置模板再匹配，或每次用请求中的模板（由实现决定，建议支持请求内 template_image）。
  - `ocr_only`：使用已加载的 OCR 引擎对 `params.image` 整图识别。
  - `tm_then_ocr`：先用 TM 在场景图中匹配模板，对每个匹配区域裁剪后调用 OCR，汇总为 regions。
- **图像格式**：Base64 解码后服务端以 BGR 或 RGB 处理均可，与引擎一致即可；顶点坐标与客户端约定为图像坐标系（左上为原点）。

---

## 6. 示例（HTTP）

**请求：ocr_only**

```http
POST /api HTTP/1.1
Host: localhost:8080
Content-Type: application/json

{
  "instruction": "ocr_only",
  "params": {
    "image": "/9j/4AAQSkZJRg..."
  },
  "id": 1
}
```

**响应**

```json
{
  "jsonrpc": "2.0",
  "result": {
    "instruction": "ocr_only",
    "blocks": [
      {
        "box": [[10,20],[100,20],[100,40],[10,40]],
        "text": "示例",
        "confidence": 0.96,
        "box_score": 0.9
      }
    ],
    "count": 1,
    "detect_time_ms": 52.1
  },
  "id": 1
}
```

---

本文档为 C++ server 与 Python server 及所有客户端的共同契约；新增字段或可选参数时请在此文档中补充说明。
