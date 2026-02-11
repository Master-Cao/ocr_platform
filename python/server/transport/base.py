# -*- coding: utf-8 -*-
"""
传输层抽象：收到请求后调用 callback(payload_dict) -> response_dict，由具体实现负责序列化与回写。
所有传输（HTTP、MQTT、ZeroMQ）共用同一协议，见 proto/api.md。
"""
from abc import ABC, abstractmethod


class ITransport(ABC):
    """传输层接口。"""

    @abstractmethod
    def start(self):
        """启动监听。"""
        pass

    @abstractmethod
    def stop(self):
        """停止。"""
        pass


# 约定：各传输实现接收 dispatch_callback(body: dict) -> dict，
# body 含 instruction / params / id，返回完整 JSON-RPC 响应体（result 或 error）。
