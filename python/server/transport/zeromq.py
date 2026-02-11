# -*- coding: utf-8 -*-
"""
ZeroMQ 传输占位：REP socket 或 ROUTER/DEALER，收到一帧 JSON 请求后交给 dispatch_callback，
将返回的 JSON 发回。
实现时可使用 pyzmq，协议与 proto/api.md 一致。
"""
import logging
import json
import threading

logger = logging.getLogger(__name__)

# 占位：未实现时仅打印日志并返回，不阻塞
_ZMQ_IMPLEMENTED = False


def run_zeromq(dispatch_callback, config, ready_event=None):
    """
    在调用线程中运行 ZeroMQ 监听（阻塞）。
    :param dispatch_callback: (body: dict) -> dict，与 HTTP 使用同一 dispatcher
    :param config: transport 配置 dict，含 zeromq_enabled, 以及 [zeromq] 的 bind_address 等
    :param ready_event: 可选 threading.Event，就绪后 set()
    """
    if not _ZMQ_IMPLEMENTED:
        logger.info(
            "ZeroMQ transport reserved. To enable: install pyzmq, implement "
            "server.transport.zeromq (REP socket recv JSON -> dispatch_callback -> send JSON), "
            "then set _ZMQ_IMPLEMENTED=True."
        )
        if ready_event:
            ready_event.set()
        return

    # 实现示例逻辑（占位）：
    # import zmq
    # ctx = zmq.Context()
    # sock = ctx.socket(zmq.REP)
    # sock.bind(config.get("zeromq_bind_address", "tcp://*:5555"))
    # while True: msg = sock.recv_string(); body = json.loads(msg); sock.send_string(json.dumps(dispatch_callback(body)))
    if ready_event:
        ready_event.set()
    return


def start_zeromq_thread(dispatch_callback, config):
    """
    在后台线程中启动 ZeroMQ（若已实现）。若未实现则仅打日志，不启动线程。
    :return: threading.Thread 或 None
    """
    if not config.get("zeromq_enabled", "false").lower() in ("true", "1", "yes"):
        return None
    if not _ZMQ_IMPLEMENTED:
        logger.info("ZeroMQ disabled (reserved). Set zeromq_enabled=true and implement zeromq.py to enable.")
        return None
    ready = threading.Event()
    t = threading.Thread(target=run_zeromq, args=(dispatch_callback, config), kwargs={"ready_event": ready})
    t.daemon = True
    t.start()
    ready.wait(timeout=5.0)
    return t
