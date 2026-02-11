# C++ 服务端其他传输接口预留

当前已实现 **HTTP**（见 `main.cpp` 中 `handle_api`）。以下为 MQTT、ZeroMQ 等扩展预留位置与约定。

## 协议统一

所有传输使用同一请求/响应格式，见项目根目录 **proto/api.md**：

- 请求：JSON，含 `instruction`（tm_only / ocr_only / tm_then_ocr）、`params`、`id`
- 响应：JSON-RPC 2.0 风格，`result` 或 `error` + `id`

## 预留位置

- **main.cpp**：在 `warmup()` 之后、`httplib::Server` 之前，可在此：
  - 启动 MQTT 工作线程（订阅请求 topic → 解析 JSON → 调用与 `handle_api` 相同的分发逻辑 → 发布响应 topic）
  - 启动 ZeroMQ 工作线程（REP 或 ROUTER/DEALER，收一帧 JSON → 同上 → 回写一帧 JSON）

## 实现建议

- **MQTT**：使用 libmosquitto 或 Eclipse Paho C；从配置或命令行读取 broker、request_topic、response_topic；收到消息后 `nlohmann::json::parse` → 与现有 `handle_api` 相同的 instruction 分支（可抽成 `dispatch(body)` 函数）→ 序列化响应并发布。
- **ZeroMQ**：使用 libzmq；bind 地址可配置（如 `tcp://*:5555`）；REP 模式下 recv 一帧字符串 → `dispatch(body)` → send 一帧字符串。

可将 `handle_api` 中的“解析 body → 按 instruction 调用 handle_* → 组装响应”抽成独立函数 `std::string dispatch(const std::string& body)`，供 HTTP、MQTT、ZeroMQ 共用。
