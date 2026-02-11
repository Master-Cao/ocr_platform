# -*- coding: utf-8 -*-
"""
MQTT 传输占位：订阅请求 topic，将 payload 作为 JSON 请求交给 dispatch_callback，
将返回的 JSON 发到响应 topic（或 reply_to）。
实现时可使用 paho-mqtt 或 aiomqtt，协议与 proto/api.md 一致。
"""
import logging
import json
import threading

logger = logging.getLogger(__name__)

# 占位：未实现时仅打印日志并返回，不阻塞
_MQTT_IMPLEMENTED = False


def run_mqtt(dispatch_callback, config, ready_event=None):
    """
    在调用线程中运行 MQTT 监听（阻塞）。
    :param dispatch_callback: (body: dict) -> dict，与 HTTP 使用同一 dispatcher
    :param config: transport 配置 dict，含 mqtt_enabled, 以及 [mqtt] 的 broker, port, request_topic, response_topic 等
    :param ready_event: 可选 threading.Event，就绪后 set()
    """
    if not _MQTT_IMPLEMENTED:
        logger.info(
            "MQTT transport reserved. To enable: install paho-mqtt, implement "
            "server.transport.mqtt (subscribe request_topic -> dispatch_callback -> publish response_topic), "
            "then set _MQTT_IMPLEMENTED=True."
        )
        if ready_event:
            ready_event.set()
        return

    # 实现示例逻辑（占位）：
    # import paho.mqtt.client as mqtt
    # client = mqtt.Client()
    # client.on_message = lambda c, u, msg: publish_response(dispatch_callback(json.loads(msg.payload)))
    # client.connect(config.get("mqtt_broker", "localhost"), config.get("mqtt_port", 1883))
    # client.subscribe(config.get("mqtt_request_topic", "ocr/request"))
    # client.loop_forever()
    if ready_event:
        ready_event.set()
    return


def start_mqtt_thread(dispatch_callback, config):
    """
    在后台线程中启动 MQTT（若已实现）。若未实现则仅打日志，不启动线程。
    :return: threading.Thread 或 None
    """
    if not config.get("mqtt_enabled", "false").lower() in ("true", "1", "yes"):
        return None
    if not _MQTT_IMPLEMENTED:
        logger.info("MQTT disabled (reserved). Set mqtt_enabled=true and implement mqtt.py to enable.")
        return None
    ready = threading.Event()
    t = threading.Thread(target=run_mqtt, args=(dispatch_callback, config), kwargs={"ready_event": ready})
    t.daemon = True
    t.start()
    ready.wait(timeout=5.0)
    return t
