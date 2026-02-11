# -*- coding: utf-8 -*-
"""
HTTP 传输：POST /api，Body 为 JSON 请求，响应为 JSON-RPC 格式。
"""
import logging
from fastapi import FastAPI, Request, HTTPException
from fastapi.responses import JSONResponse

logger = logging.getLogger(__name__)


def create_http_app(dispatch_callback):
    """
    dispatch_callback(body: dict) -> dict，返回完整 JSON-RPC 响应体。
    """
    app = FastAPI(title="QT_OCR_Service", version="1.0")

    @app.post("/api")
    async def api(request: Request):
        try:
            body = await request.json()
        except Exception as e:
            return JSONResponse(
                status_code=200,
                content={
                    "jsonrpc": "2.0",
                    "error": {"code": -32700, "message": f"Parse error: {e}"},
                    "id": None,
                },
            )
        if not isinstance(body, dict):
            return JSONResponse(
                status_code=200,
                content={
                    "jsonrpc": "2.0",
                    "error": {"code": -32600, "message": "Invalid Request"},
                    "id": None,
                },
            )
        try:
            response = dispatch_callback(body)
        except Exception as e:
            logger.exception("Dispatch failed")
            response = {
                "jsonrpc": "2.0",
                "error": {"code": -32603, "message": str(e)},
                "id": body.get("id"),
            }
        return JSONResponse(status_code=200, content=response)

    @app.get("/health")
    async def health():
        return {"status": "ok"}

    return app
