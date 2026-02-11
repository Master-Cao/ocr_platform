# -*- coding: utf-8 -*-
"""
Python 服务端入口：加载 TM + OCR、预热、启动 HTTP（及可选 MQTT/ZeroMQ）。
用法: 在项目 python 根目录执行
  python -m server.main
  或
  python server/main.py
"""
import os
import sys
import logging
import configparser

# 确保 python 根在 path 中
_TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _TOP not in sys.path:
    sys.path.insert(0, _TOP)

from server.engine_context import EngineContext
from server.dispatcher import dispatch
from server.transport.http import create_http_app
from server.transport.mqtt import start_mqtt_thread
from server.transport.zeromq import start_zeromq_thread

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)
logger = logging.getLogger(__name__)


def load_config():
    """加载 server/config/server.conf，若不存在则使用默认。"""
    default = {
        "server": {"host": "0.0.0.0", "port": "8080"},
        "ocr": {"models_dir": "", "use_gpu": "false"},
        "transport": {
            "http_enabled": "true",
            "mqtt_enabled": "false",
            "zeromq_enabled": "false",
        },
        "mqtt": {
            "broker": "localhost",
            "port": "1883",
            "request_topic": "ocr/request",
            "response_topic": "ocr/response",
        },
        "zeromq": {"bind_address": "tcp://*:5555"},
    }
    paths = [
        os.path.join(_TOP, "server", "config", "server.conf"),
        os.path.join(os.getcwd(), "server", "config", "server.conf"),
        "server/config/server.conf",
    ]
    cfg = configparser.ConfigParser()
    for p in paths:
        if os.path.isfile(p):
            cfg.read(p, encoding="utf-8")
            break
    result = {}
    for section, keys in default.items():
        result[section] = {}
        for k, v in keys.items():
            try:
                result[section][k] = cfg.get(section, k)
            except (configparser.NoSectionError, configparser.NoOptionError):
                result[section][k] = v
    # 合并为传输层统一 config（供 MQTT/ZeroMQ 线程使用）
    result["transport_flat"] = {
        "http_enabled": result["transport"].get("http_enabled", "true"),
        "mqtt_enabled": result["transport"].get("mqtt_enabled", "false"),
        "zeromq_enabled": result["transport"].get("zeromq_enabled", "false"),
        "mqtt_broker": result["mqtt"].get("broker", "localhost"),
        "mqtt_port": result["mqtt"].get("port", "1883"),
        "mqtt_request_topic": result["mqtt"].get("request_topic", "ocr/request"),
        "mqtt_response_topic": result["mqtt"].get("response_topic", "ocr/response"),
        "zeromq_bind_address": result["zeromq"].get("bind_address", "tcp://*:5555"),
    }
    return result


def main():
    config = load_config()
    ocr_dir = config["ocr"].get("models_dir") or None
    if ocr_dir and not os.path.isdir(ocr_dir):
        logger.warning("OCR models_dir not found: %s, using onnxocr defaults", ocr_dir)
        ocr_dir = None
    use_gpu = config["ocr"].get("use_gpu", "false").lower() in ("true", "1", "yes")

    logger.info("Loading engines...")
    engine = EngineContext(ocr_models_dir=ocr_dir, use_gpu=use_gpu)
    engine.load()
    engine.warmup()
    logger.info("Engines ready.")

    def dispatch_callback(body):
        return dispatch(engine, body)

    transport_flat = config.get("transport_flat", {})

    # 预留：MQTT / ZeroMQ 与 HTTP 共用同一 dispatch_callback，协议见 proto/api.md
    start_mqtt_thread(dispatch_callback, transport_flat)
    start_zeromq_thread(dispatch_callback, transport_flat)

    app = create_http_app(dispatch_callback)
    host = config["server"].get("host", "0.0.0.0")
    port = int(config["server"].get("port", "8080"))

    import uvicorn
    logger.info("HTTP server on %s:%s", host, port)
    uvicorn.run(app, host=host, port=port)


if __name__ == "__main__":
    main()
