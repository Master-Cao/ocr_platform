# QT_OCR_Service

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C++-17-00599C?logo=cplusplus)](https://isocpp.org/)
[![Python](https://img.shields.io/badge/Python-3.8+-3776AB?logo=python&logoColor=white)](https://www.python.org/)
[![CMake](https://img.shields.io/badge/CMake-3.14+-064F8C?logo=cmake)](https://cmake.org/)
[![OpenCV](https://img.shields.io/badge/OpenCV-4.x-5C3EE8?logo=opencv)](https://opencv.org/)
[![ONNX Runtime](https://img.shields.io/badge/ONNX%20Runtime-1.x-000000?logo=onnx)](https://onnxruntime.ai/)
[![FastAPI](https://img.shields.io/badge/FastAPI-0.100+-009688?logo=fastapi)](https://fastapi.tiangolo.com/)

OCR 文字检测识别与模板匹配服务。C++ 与 Python 双版本实现，部署时任选其一，对外提供统一的 JSON 接口。支持 x86_64 与 aarch64。

## 技术栈

| 类别 | 技术 |
|------|------|
| **语言** | C++17、Python 3.8+ |
| **构建** | CMake |
| **视觉 / 推理** | OpenCV、ONNX Runtime |
| **服务端** | cpp-httplib（C++）、FastAPI + Uvicorn（Python） |
| **数据格式** | JSON、Base64（图片） |
| **协议** | HTTP（已实现）、MQTT / ZeroMQ（预留） |
| **客户端（规划）** | Avalonia、浏览器 |

## 架构

```
                        proto/api.md（统一协议）
                               |
            ┌──────────────────┼──────────────────┐
            |                  |                   |
       cpp/server         python/server        （客户端）
      C++ HTTP 服务       Python HTTP 服务    ui/ web/
            |                  |
     ┌──────┴──────┐     ┌────┴────┐
     |             |     |         |
  OcrDetect  templatematch  onnxocr  OpenCV TM
  (ONNX RT)   (OpenCV)    (ONNX RT)
```

- 两套服务端实现同一协议，部署时**二选一**
- 服务端启动时加载 OCR 与模板匹配模型，执行预热后等待请求
- 通信层支持 HTTP（已实现）、MQTT / ZeroMQ（接口已预留）

## 指令

| 指令 | 说明 |
|------|------|
| `ocr_only` | 对输入图像做文字检测 + 方向分类 + 识别 |
| `tm_only` | 在场景图中匹配模板，返回位置与得分 |
| `tm_then_ocr` | 先模板匹配定位区域，再对每个区域做 OCR |

完整的请求/响应格式、字段定义与错误码见 [proto/api.md](proto/api.md)。

## 项目结构

```
QT_OCR_Service/
├── proto/                        # 统一 API 协议文档
│   └── api.md
├── cpp/                          # C++ 版本
│   ├── OcrDetect/                #   OCR 库（ONNX Runtime + OpenCV）
│   ├── templatematch/            #   模板匹配库
│   ├── examples/                 #   命令行示例
│   ├── server/                   #   HTTP 服务端
│   │   ├── config/server.conf    #     服务配置
│   │   └── transport/            #     MQTT/ZeroMQ 预留
│   ├── CMakeLists.txt
│   └── build.sh
├── python/                       # Python 版本
│   ├── onnxocr/                  #   OCR 库（ONNX + OpenCV）
│   ├── server/                   #   HTTP 服务端
│   │   ├── config/server.conf    #     服务配置
│   │   └── transport/            #     HTTP 实现 + MQTT/ZeroMQ 预留
│   └── requirements.txt
├── ui/                           # Avalonia 桌面客户端（规划中）
├── web/                          # 浏览器客户端（规划中）
└── LICENSE                       # Apache 2.0
```

## 快速开始

### C++ 版本

**环境**：CMake 3.14+、C++17、OpenCV、ONNX Runtime

```bash
cd cpp
./build.sh                        # 或 mkdir build && cd build && cmake .. && make
./build/bin/ocr_server --models ./OcrDetect/models --config-dir ./build/config
```

### Python 版本

**环境**：Python 3.8+

```bash
cd python
pip install -r requirements.txt
pip install -r server/requirements.txt
python -m server.main
```

服务默认监听 `0.0.0.0:8080`，可在 `server/config/server.conf` 中修改。

### 调用示例

```bash
# 健康检查
curl http://localhost:8080/health

# OCR 识别
curl -X POST http://localhost:8080/api \
  -H "Content-Type: application/json" \
  -d '{
    "instruction": "ocr_only",
    "params": { "image": "<base64 编码的图片>" },
    "id": 1
  }'
```

响应格式：

```json
{
  "jsonrpc": "2.0",
  "result": {
    "instruction": "ocr_only",
    "blocks": [
      {
        "box": [[10,20],[100,20],[100,40],[10,40]],
        "text": "识别结果",
        "confidence": 0.96
      }
    ],
    "count": 1
  },
  "id": 1
}
```

## 配置

两套服务端的配置文件结构一致：

| 配置段 | 说明 |
|--------|------|
| `[server]` | 监听地址 `host`、端口 `port` |
| `[ocr]` | 模型目录、是否使用 GPU（仅 Python） |
| `[transport]` | 各传输开关：`http_enabled`、`mqtt_enabled`、`zeromq_enabled` |
| `[mqtt]` | Broker 地址、端口、请求/响应 Topic（预留） |
| `[zeromq]` | Bind 地址（预留） |

C++ 端还通过 `--config-dir` 读取 `ocrdetect.conf` 与 `templatematch.conf` 来调整 OCR 和模板匹配参数。

## 开发进度

### 已完成

- [x] OCR 引擎（C++ / Python）
- [x] 模板匹配引擎（C++ 完整算法 / Python OpenCV 简易版）
- [x] 统一 API 协议定义（proto/api.md）
- [x] C++ HTTP 服务端（加载、预热、指令分发）
- [x] Python HTTP 服务端（加载、预热、指令分发）
- [x] 服务端配置文件
- [x] MQTT / ZeroMQ 接口与配置预留

### 进行中

- [ ] Avalonia 桌面客户端（`ui/`）
- [ ] 浏览器客户端（`web/`，仅 HTTP）
- [ ] MQTT 传输实现
- [ ] ZeroMQ 传输实现
- [ ] aarch64 部署文档与脚本

## 许可证

[Apache License 2.0](LICENSE)
