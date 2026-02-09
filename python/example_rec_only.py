#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
仅识别示例：仅用 rec.onnx 对裁剪图做识别（不经过检测/方向分类）。
用法: python example_rec_only.py --model_dir onnxocr/models/ppocrv4  image1.png [image2.png ...]
      python example_rec_only.py --rec rec.onnx --keys ppocr_keys_v1.txt  image1.png
"""
import argparse
import math
import os
import sys

import cv2
import numpy as np
import onnxruntime

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if _SCRIPT_DIR not in sys.path:
    sys.path.insert(0, _SCRIPT_DIR)
from onnxocr.rec_postprocess import CTCLabelDecode

REC_IMG_SHAPE = [3, 48, 320]


def resize_norm_img(img, max_wh_ratio=None):
    imgC, imgH, imgW = REC_IMG_SHAPE[0], REC_IMG_SHAPE[1], REC_IMG_SHAPE[2]
    assert img.ndim == 3 and img.shape[2] == 3
    h, w = img.shape[0], img.shape[1]
    if max_wh_ratio is None:
        max_wh_ratio = max(imgW / imgH, w / float(h))
    else:
        max_wh_ratio = max(max_wh_ratio, w / float(h))
    imgW = int(imgH * max_wh_ratio)
    ratio = w / float(h)
    resized_w = imgW if math.ceil(imgH * ratio) > imgW else int(math.ceil(imgH * ratio))
    resized_image = cv2.resize(img, (resized_w, imgH))
    resized_image = resized_image.astype(np.float32)
    resized_image = resized_image.transpose((2, 0, 1)) / 255.0
    resized_image -= 0.5
    resized_image /= 0.5
    padding_im = np.zeros((imgC, imgH, imgW), dtype=np.float32)
    padding_im[:, :, 0:resized_w] = resized_image
    return padding_im


def run_rec(rec_path, keys_path, image_paths):
    if not os.path.isfile(rec_path):
        print(f"错误: 未找到 rec 模型: {rec_path}", file=sys.stderr)
        return 1
    if not os.path.isfile(keys_path):
        print(f"错误: 未找到 keys 文件: {keys_path}", file=sys.stderr)
        return 2

    session = onnxruntime.InferenceSession(rec_path, providers=["CPUExecutionProvider"])
    input_name = session.get_inputs()[0].name
    output_names = [o.name for o in session.get_outputs()]
    postprocess_op = CTCLabelDecode(character_dict_path=keys_path, use_space_char=True)

    for path in image_paths:
        if not os.path.isfile(path):
            print(f"跳过（不存在）: {path}", file=sys.stderr)
            continue
        img = cv2.imread(path)
        if img is None:
            print(f"跳过（无法读取）: {path}", file=sys.stderr)
            continue
        norm_img = resize_norm_img(img)
        norm_batch = np.expand_dims(norm_img, axis=0).astype(np.float32)
        outputs = session.run(output_names, {input_name: norm_batch})
        preds = outputs[0]
        if preds.ndim != 3:
            print(f"  {path}: 未预期的输出维度", file=sys.stderr)
            continue
        preds_idx = preds.argmax(axis=2)
        preds_prob = preds.max(axis=2)
        result_list = postprocess_op.decode(preds_idx, preds_prob, is_remove_duplicate=True)
        text, score = result_list[0]
        print(f"  {path} -> \"{text}\" (置信度: {score:.4f})")
    return 0


def main():
    parser = argparse.ArgumentParser(description="仅识别示例：rec.onnx 对裁剪图识别")
    g = parser.add_mutually_exclusive_group(required=True)
    g.add_argument("--model_dir", type=str, help="模型目录（含 rec/rec.onnx 或 rec.onnx 及 keys）")
    g.add_argument("--rec", type=str, help="rec.onnx 路径（需同时 --keys）")
    parser.add_argument("--keys", type=str, help="字典文件路径")
    parser.add_argument("images", nargs="+", help="裁剪图路径")
    args = parser.parse_args()

    rec_path = None
    keys_path = None
    if args.model_dir:
        base = os.path.abspath(args.model_dir)
        rec_dir = os.path.join(base, "rec")
        if os.path.isfile(os.path.join(rec_dir, "rec.onnx")):
            rec_path = os.path.join(rec_dir, "rec.onnx")
        elif os.path.isfile(os.path.join(base, "rec.onnx")):
            rec_path = os.path.join(base, "rec.onnx")
        else:
            print(f"错误: 未找到 rec.onnx", file=sys.stderr)
            return 1
        for name in ("ppocr_keys_v1.txt", "keys.txt"):
            for d in (base, os.path.dirname(rec_path)):
                p = os.path.join(d, name)
                if os.path.isfile(p):
                    keys_path = p
                    break
            if keys_path:
                break
        if not keys_path:
            alt = os.path.join(os.path.dirname(base), "ch_ppocr_server_v2.0", "ppocr_keys_v1.txt")
            keys_path = alt if os.path.isfile(alt) else None
        if not keys_path:
            print("错误: 未找到 ppocr_keys_v1.txt 或 keys.txt", file=sys.stderr)
            return 2
    else:
        rec_path = args.rec
        keys_path = args.keys
        if not keys_path:
            print("错误: 使用 --rec 时需指定 --keys", file=sys.stderr)
            return 2
    return run_rec(rec_path, keys_path, args.images)


if __name__ == "__main__":
    sys.exit(main())
