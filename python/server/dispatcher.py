# -*- coding: utf-8 -*-
"""
根据 instruction 分发到引擎，返回符合 proto 的 result 或 error。
"""
import logging
from .engine_context import EngineContext, decode_image_from_base64

logger = logging.getLogger(__name__)

# 错误码与 proto 一致
PARSE_ERROR = -32700
INVALID_REQUEST = -32600
INVALID_PARAMS = -32602
INTERNAL_ERROR = -32603
BUSINESS_ERROR = -32000


def dispatch(engine: EngineContext, body: dict) -> dict:
    """
    处理单条请求 body，返回完整 JSON-RPC 响应体（含 result 或 error、id）。
    body 应包含: instruction, params, id
    """
    req_id = body.get("id")
    if "instruction" not in body:
        return _error(INVALID_REQUEST, "Missing instruction", req_id)
    instruction = body.get("instruction")
    params = body.get("params")
    if not isinstance(params, dict):
        return _error(INVALID_PARAMS, "params must be an object", req_id)

    try:
        if instruction == "tm_only":
            result = _handle_tm_only(engine, params)
        elif instruction == "ocr_only":
            result = _handle_ocr_only(engine, params)
        elif instruction == "tm_then_ocr":
            result = _handle_tm_then_ocr(engine, params)
        else:
            return _error(INVALID_REQUEST, f"Unknown instruction: {instruction}", req_id)
    except ValueError as e:
        return _error(INVALID_PARAMS, str(e), req_id)
    except Exception as e:
        logger.exception("Dispatch error")
        return _error(INTERNAL_ERROR, str(e), req_id)

    return {"jsonrpc": "2.0", "result": result, "id": req_id}


def _error(code: int, message: str, req_id) -> dict:
    return {
        "jsonrpc": "2.0",
        "error": {"code": code, "message": message},
        "id": req_id,
    }


def _get_image(params: dict, key: str) -> "np.ndarray":
    b64 = params.get(key)
    if not b64:
        raise ValueError(f"Missing param: {key}")
    return decode_image_from_base64(b64)


def _handle_tm_only(engine: EngineContext, params: dict) -> dict:
    scene = _get_image(params, "scene_image")
    template_b64 = params.get("template_image")
    if not template_b64:
        raise ValueError("tm_only requires template_image")
    template = decode_image_from_base64(template_b64)
    engine.tm_set_template(template)
    tm_params = params.get("tm_params") or {}
    out = engine.tm_match(scene, tm_params)
    out["instruction"] = "tm_only"
    return out


def _handle_ocr_only(engine: EngineContext, params: dict) -> dict:
    image = _get_image(params, "image")
    ocr_options = params.get("ocr_options") or {}
    out = engine.ocr_detect(image, ocr_options)
    out["instruction"] = "ocr_only"
    return out


def _handle_tm_then_ocr(engine: EngineContext, params: dict) -> dict:
    scene = _get_image(params, "scene_image")
    template = _get_image(params, "template_image")
    tm_params = params.get("tm_params") or {}
    ocr_options = params.get("ocr_options") or {}
    out = engine.tm_then_ocr(scene, template, tm_params, ocr_options)
    out["instruction"] = "tm_then_ocr"
    return out
