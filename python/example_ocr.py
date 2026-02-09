#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
OCR 全流程示例：检测 + 方向分类 + 识别，并保存标注图。

用法:
  python example_ocr.py [--image 图片路径] [--model_dir 模型目录] [--out 输出图]
  工作目录建议为 python/，默认使用 onnxocr/models、onnxocr/test_images。

示例:
  python example_ocr.py
  python example_ocr.py --image onnxocr/test_images/1.jpg --out draw_ocr.jpg
"""
import argparse
import os
import sys
import time

import cv2

# 保证可找到 onnxocr（以当前脚本所在目录为 python/）
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if _SCRIPT_DIR not in sys.path:
    sys.path.insert(0, _SCRIPT_DIR)

from onnxocr.onnx_paddleocr import ONNXPaddleOcr, sav2Img


def main():
    parser = argparse.ArgumentParser(description="OCR 全流程示例")
    parser.add_argument("--image", type=str, default="onnxocr/test_images/before_roi_155426.jpg",
                        help="输入图片路径")
    parser.add_argument("--model_dir", type=str, default=None,
                        help="模型目录（默认使用 onnxocr 内置默认）")
    parser.add_argument("--out", type=str, default="draw_ocr.jpg", help="输出标注图路径")
    parser.add_argument("--use_gpu", action="store_true", help="使用 GPU")
    args = parser.parse_args()

    kwargs = {"use_angle_cls": True, "use_gpu": args.use_gpu}
    if args.model_dir:
        m = os.path.abspath(args.model_dir)
        kwargs["det_model_dir"] = os.path.join(m, "det", "det.onnx")
        kwargs["rec_model_dir"] = os.path.join(m, "rec", "rec.onnx")
        kwargs["cls_model_dir"] = os.path.join(m, "cls", "cls.onnx")
        for keys_name in [os.path.join(m, "ppocr_keys_v1.txt"), os.path.join(m, "..", "ppocr_keys_v1.txt")]:
            if os.path.isfile(keys_name):
                kwargs["rec_char_dict_path"] = keys_name
                break

    model = ONNXPaddleOcr(**kwargs)
    if not os.path.isfile(args.image):
        print(f"错误: 图片不存在: {args.image}", file=sys.stderr)
        return 1
    img = cv2.imread(args.image)
    if img is None:
        print(f"错误: 无法读取图片: {args.image}", file=sys.stderr)
        return 1

    t0 = time.time()
    result = model.ocr(img)
    elapsed = time.time() - t0

    print(f"total time: {elapsed:.3f}")
    print(f"result: {len(result[0])} boxes")
    for line in result[0]:
        print(line)
    sav2Img(img, result, name=args.out)
    print(f"saved: {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
