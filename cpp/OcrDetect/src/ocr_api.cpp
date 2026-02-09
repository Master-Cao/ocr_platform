/**
 * OCR 检测 C API 实现
 */
#include "../include/ocrdetect/ocr_api.h"
#include "../include/ocrdetect/export.h"
#include "OcrLite.h"
#include "OcrUtils.h"
#include <opencv2/opencv.hpp>
#include <cstring>
#include <string>

#ifdef _WIN32
#  define PATH_SEP "\\"
#else
#  define PATH_SEP "/"
#endif

static std::string join_path(const char* dir, const char* name) {
  std::string s(dir);
  if (!s.empty() && s.back() != '/' && s.back() != '\\') s += PATH_SEP;
  s += name;
  return s;
}

OCRDETECT_OCR_API OCR_Handle OCRDETECT_OCR_CALL ocr_create(const char* models_dir) {
  if (!models_dir || !models_dir[0]) return nullptr;
  try {
    OcrLite* p = new OcrLite();
    p->initLogger(false, false, false);
    std::string det_path = join_path(models_dir, "det.onnx");
    std::string cls_path = join_path(models_dir, "cls.onnx");
    std::string rec_path = join_path(models_dir, "rec.onnx");
    std::string keys_path = join_path(models_dir, "ppocr_keys_v1.txt");
    if (!isFileExists(keys_path))
      keys_path = join_path(models_dir, "keys.txt");
    if (!p->initModels(det_path, cls_path, rec_path, keys_path)) {
      delete p;
      return nullptr;
    }
    return static_cast<void*>(p);
  } catch (...) {
    return nullptr;
  }
}

OCRDETECT_OCR_API void OCRDETECT_OCR_CALL ocr_destroy(OCR_Handle h) {
  if (h) delete static_cast<OcrLite*>(h);
}

OCRDETECT_OCR_API void OCRDETECT_OCR_CALL ocr_set_num_threads(OCR_Handle h, int n) {
  if (h) static_cast<OcrLite*>(h)->setNumThread(n);
}

OCRDETECT_OCR_API void OCRDETECT_OCR_CALL ocr_set_preprocess_save_path(OCR_Handle h, const char* path) {
  if (h) static_cast<OcrLite*>(h)->setPreprocessSavePath(path ? path : "");
}

OCRDETECT_OCR_API void OCRDETECT_OCR_CALL ocr_set_part_imgs_save_path(OCR_Handle h, const char* path) {
  if (h) static_cast<OcrLite*>(h)->setPartImagesSavePath(path ? path : "");
}

OCRDETECT_OCR_API int OCRDETECT_OCR_CALL ocr_detect(
  OCR_Handle h,
  const unsigned char* image_data, int width, int height, int channels,
  const OCR_Options* options,
  OCR_TextBlock* results, int max_results) {
  if (!h || !image_data || !results || max_results <= 0) return -1;
  cv::Mat mat;
  if (channels == 1)
    mat = cv::Mat(height, width, CV_8UC1);
  else if (channels == 3)
    mat = cv::Mat(height, width, CV_8UC3);
  else
    return -2;
  size_t copy_len = static_cast<size_t>(width * height * channels);
  std::memcpy(mat.data, image_data, copy_len);

  int padding = 0, short_side_len = 960;
  float box_score_thresh = 0.6f, box_thresh = 0.3f, un_clip_ratio = 2.0f;
  bool do_angle = true, most_angle = true;
  if (options) {
    padding = options->padding;
    short_side_len = options->short_side_len;
    box_score_thresh = options->box_score_thresh;
    box_thresh = options->box_thresh;
    un_clip_ratio = options->un_clip_ratio;
    do_angle = (options->do_angle != 0);
    most_angle = (options->most_angle != 0);
  }

  OcrResult res = static_cast<OcrLite*>(h)->detect(mat, padding, short_side_len,
    box_score_thresh, box_thresh, un_clip_ratio, do_angle, most_angle);

  int n = static_cast<int>(res.textBlocks.size());
  if (n > max_results) n = max_results;
  for (int i = 0; i < n; i++) {
    const TextBlock& b = res.textBlocks[i];
    OCR_TextBlock* out = results + i;
    size_t pts = b.boxPoint.size();
    if (pts > 4) pts = 4;
    for (size_t k = 0; k < pts; k++) {
      out->box[k].x = b.boxPoint[k].x;
      out->box[k].y = b.boxPoint[k].y;
    }
    for (size_t k = pts; k < 4; k++) { out->box[k].x = 0; out->box[k].y = 0; }
    out->box_score = b.boxScore;
    out->text = nullptr;
    if (!b.text.empty()) {
      out->text = static_cast<char*>(malloc(b.text.size() + 1));
      if (out->text) {
        std::strcpy(out->text, b.text.c_str());
      }
    }
    if (b.charScores.empty()) out->confidence = 0.f;
    else {
      float sum = 0;
      for (float s : b.charScores) sum += s;
      out->confidence = sum / static_cast<float>(b.charScores.size());
    }
  }
  return n;
}

OCRDETECT_OCR_API void OCRDETECT_OCR_CALL ocr_free_text(char* text) {
  if (text) free(text);
}

/* ============================================================
 * C++ 直接 cv::Mat 接口 —— 与 OcrService 调用方式完全一致
 * ============================================================ */
OCRDETECT_OCR_API int OCRDETECT_OCR_CALL ocr_detect_mat(
  OCR_Handle h,
  const cv::Mat& image,
  const OCR_Options* options,
  std::vector<OCR_TextBlockCpp>& out) {
  if (!h || image.empty()) return -1;

  int padding = 0, short_side_len = 960;
  float box_score_thresh = 0.6f, box_thresh = 0.3f, un_clip_ratio = 2.0f;
  bool do_angle = true, most_angle = true;
  if (options) {
    padding = options->padding;
    short_side_len = options->short_side_len;
    box_score_thresh = options->box_score_thresh;
    box_thresh = options->box_thresh;
    un_clip_ratio = options->un_clip_ratio;
    do_angle = (options->do_angle != 0);
    most_angle = (options->most_angle != 0);
  }

  // 与 OcrService 中 ocr_engine_->detect(image, ...) 完全一致：
  // 直接传递 cv::Mat 引用，不经过原始字节拆解/重建
  OcrResult res = static_cast<OcrLite*>(h)->detect(image, padding, short_side_len,
    box_score_thresh, box_thresh, un_clip_ratio, do_angle, most_angle);

  out.clear();
  out.reserve(res.textBlocks.size());
  for (size_t i = 0; i < res.textBlocks.size(); ++i) {
    const TextBlock& b = res.textBlocks[i];
    OCR_TextBlockCpp tb;
    size_t pts = b.boxPoint.size();
    if (pts > 4) pts = 4;
    for (size_t k = 0; k < pts; k++) {
      tb.box[k][0] = static_cast<float>(b.boxPoint[k].x);
      tb.box[k][1] = static_cast<float>(b.boxPoint[k].y);
    }
    for (size_t k = pts; k < 4; k++) { tb.box[k][0] = 0; tb.box[k][1] = 0; }
    tb.box_score = b.boxScore;
    tb.text = b.text;
    if (b.charScores.empty()) {
      tb.confidence = 0.f;
    } else {
      float sum = 0;
      for (float s : b.charScores) sum += s;
      tb.confidence = sum / static_cast<float>(b.charScores.size());
    }
    out.push_back(std::move(tb));
  }
  return static_cast<int>(out.size());
}
