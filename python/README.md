# QT_OCR_Service Python（OCR）

本目录为 Python 版 OCR 服务，基于 OnnxOCR（PaddleOCR ONNX 模型）。

## 依赖

```bash
pip install -r requirements.txt
```

## 模型与资源

- 模型默认路径：`onnxocr/models/`（如 ppocrv4：det、cls、rec、ppocr_keys_v1.txt）
- 示例图片：`onnxocr/test_images/`

## 运行示例

建议在 **python/** 目录下执行，以便相对路径正确。

```bash
# 全流程 OCR（检测 + 方向分类 + 识别），输出总耗时与结果并保存标注图
python example_ocr.py
python example_ocr.py --image onnxocr/test_images/1.jpg --out draw_ocr.jpg
python example_ocr.py --model_dir onnxocr/models/ppocrv4 --image your.png

# 仅识别（仅用 rec.onnx 对裁剪图做识别）
python example_rec_only.py --model_dir onnxocr/models/ppocrv4  part_0.png part_1.png
python example_rec_only.py --rec onnxocr/models/ppocrv4/rec/rec.onnx --keys onnxocr/models/ppocrv4/ppocr_keys_v1.txt  part_0.png
```

## 目录结构

```
python/
├── onnxocr/           # 包源码（models、fonts、test_images、*.py）
├── requirements.txt
├── example_ocr.py     # 全流程示例
├── example_rec_only.py # 仅识别示例
└── README.md
```

## 在代码中使用

```python
from onnxocr.onnx_paddleocr import ONNXPaddleOcr, sav2Img
import cv2

model = ONNXPaddleOcr(use_angle_cls=True, use_gpu=False)
img = cv2.imread('your_image.jpg')
result = model.ocr(img)   # result[0] 为 [[box, (text, score)], ...]
sav2Img(img, result, name='draw_ocr.jpg')
```
