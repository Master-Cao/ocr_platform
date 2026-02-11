# Python 服务端

与 proto/api.md 一致的 HTTP 服务：启动时加载 TM + OCR 并预热，按指令执行 `tm_only`、`ocr_only`、`tm_then_ocr`。  
**MQTT、ZeroMQ** 接口已预留：配置项与占位实现见 `transport/mqtt.py`、`transport/zeromq.py`，实现后可在 `server.conf` 中开启并随进程启动。

## 依赖

在项目 `python/` 目录下已具备 onnxocr 依赖；服务端额外需要：

```bash
pip install fastapi "uvicorn[standard]"
```

或安装 `server/requirements.txt`（需先安装根目录 `requirements.txt` 中的 onnxocr 等）。

## 配置

- `server/config/server.conf`：监听地址/端口、OCR 模型目录、是否启用 HTTP 等。
- OCR 模型目录：若 `[ocr] models_dir` 为空，则使用 onnxocr 默认路径（如 `onnxocr/models/ppocrv4`）。

## 运行

在 **python 根目录**（即与 `server`、`onnxocr` 同级）执行：

```bash
python -m server.main
```

或：

```bash
python server/main.py
```

默认监听 `0.0.0.0:8080`。接口：

- `POST /api`：JSON 请求体，见 proto/api.md（instruction + params + id）。
- `GET /health`：健康检查，返回 `{"status":"ok"}`。

## 指令说明

- **tm_only**：请求中需 `scene_image`、`template_image`（Base64），返回匹配框列表。
- **ocr_only**：请求中需 `image`（Base64），返回文本框与识别结果。
- **tm_then_ocr**：请求中需 `scene_image`、`template_image`，先匹配再对每个区域做 OCR，返回 regions。

## 其他传输（MQTT / ZeroMQ）

在 `server/config/server.conf` 的 `[transport]` 中可设置 `mqtt_enabled`、`zeromq_enabled`；`[mqtt]`、`[zeromq]` 中可配置 broker、topic、bind 地址等。当前为占位实现，启用后仅打日志。实现步骤：

1. 在 `server/transport/mqtt.py` 中接入 paho-mqtt 或 aiomqtt，订阅 `request_topic`，将 payload 交 `dispatch_callback`，结果发到 `response_topic`，最后将 `_MQTT_IMPLEMENTED = True`。
2. 在 `server/transport/zeromq.py` 中接入 pyzmq（REP 或 ROUTER/DEALER），收一帧 JSON、调 `dispatch_callback`、回写一帧，最后将 `_ZMQ_IMPLEMENTED = True`。
