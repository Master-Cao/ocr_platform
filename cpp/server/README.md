# C++ 服务端

与 proto/api.md 一致的 HTTP 服务：启动时加载 TM + OCR、预热，按 instruction 执行 tm_only / ocr_only / tm_then_ocr。

## 配置

- **server/config/server.conf**：服务端专用配置，与 Python 端结构一致。
  - `[server]`：host、port（默认 0.0.0.0:8080）
  - `[transport]`：http_enabled、mqtt_enabled、zeromq_enabled（预留）
  - `[mqtt]` / `[zeromq]`：预留，供后续 MQTT/ZeroMQ 实现使用
- 构建时该文件会复制到 `build/config/server.conf`，与 templatematch.conf、ocrdetect.conf 同目录；运行时通过 `--config-dir` 指定该 config 目录即可一并生效。

## 构建

在 **cpp** 根目录构建（需已解压 ONNX Runtime，参见 OcrDetect/README）：

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

可执行文件在 `build/bin/ocr_server`。构建时会通过 FetchContent 拉取 nlohmann/json 与 cpp-httplib（需网络）。

## 运行

```bash
./bin/ocr_server --models <OCR模型目录> [--config-dir <配置目录>]
```

- `--models`：OCR 模型目录，内含 det.onnx、cls.onnx、rec.onnx、ppocr_keys_v1.txt。
- `--config-dir`：可选，默认 `config`，用于读取 templatematch.conf、ocrdetect.conf、**server.conf**（同目录下）。

HTTP 监听地址与端口由 server.conf 的 `[server]` 决定（默认 0.0.0.0:8080）：

- `POST /api`：JSON 请求，见 proto/api.md。
- `GET /health`：返回 `{"status":"ok"}`。

## 初始化流程

1. 加载 templatematch 与 ocrdetect 配置。
2. 创建 Matcher 与 OcrEngine，执行一次小图预热。
3. 启动 HTTP 服务，等待请求。
