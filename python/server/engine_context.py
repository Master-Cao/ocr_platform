# -*- coding: utf-8 -*-
"""
服务端引擎上下文：加载 OCR + 模板匹配，并执行预热。
Python 版本：OCR 使用 onnxocr，TM 使用 OpenCV matchTemplate（与 cpp 行为近似，可后续替换）。
"""
import os
import sys
import logging
import numpy as np
import cv2

# 保证可找到 onnxocr（server 从项目 python/ 根运行）
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_PYTHON_ROOT = os.path.dirname(_SCRIPT_DIR)
if _PYTHON_ROOT not in sys.path:
    sys.path.insert(0, _PYTHON_ROOT)

from onnxocr.onnx_paddleocr import ONNXPaddleOcr

logger = logging.getLogger(__name__)


def _decode_image(b64: str):
    """Base64 解码为 BGR 图像 (numpy ndarray)。"""
    import base64
    raw = b64
    if "," in raw:
        raw = raw.split(",", 1)[1]
    data = base64.b64decode(raw)
    arr = np.frombuffer(data, dtype=np.uint8)
    img = cv2.imdecode(arr, cv2.IMREAD_COLOR)
    if img is None:
        raise ValueError("Invalid image: base64 decode failed")
    return img


class SimpleTemplateMatcher:
    """使用 OpenCV matchTemplate 的简单模板匹配，返回与 proto 一致的 match 结构。"""

    def __init__(self, score_threshold=0.5, max_count=200):
        self.score_threshold = score_threshold
        self.max_count = max_count
        self._template_gray = None  # 当前模板（灰度）
        self._t_h = 0
        self._t_w = 0

    def set_template(self, image_bgr):
        gray = cv2.cvtColor(image_bgr, cv2.COLOR_BGR2GRAY)
        self._template_gray = gray
        self._t_h, self._t_w = gray.shape[:2]
        return True

    def match(self, scene_bgr):
        if self._template_gray is None:
            raise RuntimeError("Template not set")
        scene_gray = cv2.cvtColor(scene_bgr, cv2.COLOR_BGR2GRAY)
        result = cv2.matchTemplate(scene_gray, self._template_gray, cv2.TM_CCOEFF_NORMED)
        # 收集所有高于阈值的点，按得分降序，取前 max_count（简单实现不做 IOU 去重）
        ys, xs = np.where(result >= self.score_threshold)
        scores = result[ys, xs]
        order = np.argsort(-scores)[: self.max_count]
        matches = []
        for i in order:
            x = int(xs[i])
            y = int(ys[i])
            s = float(scores[i])
            w, h = self._t_w, self._t_h
            matches.append({
                "left_top": [float(x), float(y)],
                "right_top": [float(x + w), float(y)],
                "right_bottom": [float(x + w), float(y + h)],
                "left_bottom": [float(x), float(y + h)],
                "center": [float(x + w / 2), float(y + h / 2)],
                "angle": 0.0,
                "score": s,
            })
        return matches


class EngineContext:
    """持有 OCR 与 TM 引擎，负责加载与预热。"""

    def __init__(self, ocr_models_dir=None, use_gpu=False):
        """
        :param ocr_models_dir: OCR 模型目录，内含 det/det.onnx, rec/rec.onnx, cls/cls.onnx, ppocr_keys_v1.txt
                              若为 None 则使用 onnxocr 默认路径（相对于 python 根目录）
        :param use_gpu: 是否使用 GPU
        """
        self._ocr = None
        self._tm = SimpleTemplateMatcher()
        self._ocr_models_dir = ocr_models_dir
        self._use_gpu = use_gpu

    def load(self):
        """加载 OCR 与 TM（TM 无模型文件，仅初始化）。"""
        logger.info("Loading OCR models...")
        kwargs = {"use_gpu": self._use_gpu, "use_angle_cls": True}
        if self._ocr_models_dir:
            d = os.path.abspath(self._ocr_models_dir)
            kwargs["det_model_dir"] = os.path.join(d, "det", "det.onnx")
            kwargs["rec_model_dir"] = os.path.join(d, "rec", "rec.onnx")
            kwargs["cls_model_dir"] = os.path.join(d, "cls", "cls.onnx")
            for name in ["ppocr_keys_v1.txt", "keys.txt"]:
                keys_path = os.path.join(d, name)
                if os.path.isfile(keys_path):
                    kwargs["rec_char_dict_path"] = keys_path
                    break
        self._ocr = ONNXPaddleOcr(**kwargs)
        logger.info("OCR loaded.")
        # TM 无额外模型
        logger.info("TM (OpenCV) ready.")
        return True

    def warmup(self):
        """预热：各跑一次小图，避免首包慢。"""
        logger.info("Warmup OCR...")
        dummy = np.zeros((64, 256, 3), dtype=np.uint8)
        dummy[:] = 255
        try:
            _ = self._ocr.ocr(dummy, det=True, rec=True, cls=True)
        except Exception as e:
            logger.warning("OCR warmup exception (ignored): %s", e)
        logger.info("Warmup TM...")
        self._tm.set_template(dummy)
        _ = self._tm.match(dummy)
        logger.info("Warmup done.")

    def ocr_detect(self, image_bgr, options=None):
        """
        对整图做 OCR。返回与 proto 一致的 blocks 列表。
        options: 可选 dict，如 short_side_len, box_score_thresh 等（python onnxocr 部分参数在初始化时固定，这里仅占位）
        """
        options = options or {}
        res = self._ocr.ocr(image_bgr, det=True, rec=True, cls=True)
        if not res or not res[0]:
            return {"blocks": [], "count": 0}
        blocks = []
        for item in res[0]:
            box = item[0]  # [[x,y],...]
            text, score = item[1]
            # box 可能是 4x2，转为 4 个 [x,y]
            box_list = [ [float(p[0]), float(p[1])] for p in box ]
            blocks.append({
                "box": box_list,
                "text": text or "",
                "confidence": float(score),
                "box_score": float(score),
            })
        return {"blocks": blocks, "count": len(blocks)}

    def tm_set_template(self, image_bgr):
        self._tm.set_template(image_bgr)
        return True

    def tm_match(self, scene_bgr, tm_params=None):
        if tm_params:
            if "score_threshold" in tm_params:
                self._tm.score_threshold = float(tm_params["score_threshold"])
            if "max_count" in tm_params:
                self._tm.max_count = int(tm_params["max_count"])
        matches = self._tm.match(scene_bgr)
        return {"matches": matches, "count": len(matches)}

    def tm_then_ocr(self, scene_bgr, template_bgr, tm_params=None, ocr_options=None):
        self._tm.set_template(template_bgr)
        if tm_params:
            if "score_threshold" in tm_params:
                self._tm.score_threshold = float(tm_params["score_threshold"])
            if "max_count" in tm_params:
                self._tm.max_count = int(tm_params["max_count"])
        matches = self._tm.match(scene_bgr)
        regions = []
        for m in matches:
            # 裁剪区域
            x0 = int(m["left_top"][0])
            y0 = int(m["left_top"][1])
            x1 = int(m["right_bottom"][0])
            y1 = int(m["right_bottom"][1])
            # 边界
            h, w = scene_bgr.shape[:2]
            x0 = max(0, min(x0, w - 1))
            x1 = max(0, min(x1, w))
            y0 = max(0, min(y0, h - 1))
            y1 = max(0, min(y1, h))
            if x1 <= x0 or y1 <= y0:
                regions.append({"match": m, "ocr": {"blocks": [], "count": 0}})
                continue
            crop = scene_bgr[y0:y1, x0:x1]
            ocr_res = self.ocr_detect(crop, ocr_options)
            regions.append({"match": m, "ocr": ocr_res})
        return {"regions": regions, "match_count": len(regions)}


def decode_image_from_base64(b64: str):
    """供 dispatcher 使用。"""
    return _decode_image(b64)
