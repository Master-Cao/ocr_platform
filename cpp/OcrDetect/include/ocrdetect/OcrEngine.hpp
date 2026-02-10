/**
 * @file OcrEngine.hpp
 * @brief OCR 检测 C++ 封装
 * 
 * 采用与 OcrService 完全一致的调用方式：
 * 直接传递 cv::Mat 引用给 OcrLite::detect()，不经过原始字节拷贝
 */
#ifndef OCRDETECT_OCR_ENGINE_HPP
#define OCRDETECT_OCR_ENGINE_HPP

#include "ocr_api.h"
#include "ConfigLoader.hpp"
#include <opencv2/core.hpp>
#include <vector>
#include <string>
#include <memory>

namespace ocrdetect {

struct TextBlock {
  float box[4][2];  // 4 points (x,y)
  float box_score;
  std::string text;
  float confidence;
};

class OcrEngine {
public:
  /** 创建时直接加载模型；models_dir 含 det.onnx, cls.onnx, rec.onnx, ppocr_keys_v1.txt 或 keys.txt */
  explicit OcrEngine(const std::string& models_dir)
    : handle_(ocr_create(models_dir.c_str())) {}
  ~OcrEngine() { if (handle_) ocr_destroy(handle_); }

  OcrEngine(const OcrEngine&) = delete;
  OcrEngine& operator=(const OcrEngine&) = delete;

  bool valid() const { return handle_ != nullptr; }

  void setNumThreads(int n) { if (handle_) ocr_set_num_threads(handle_, n); }

  /** 设置预处理图像保存路径（调试用），下次 detect 时保存预处理结果 */
  void setPreprocessSavePath(const std::string& path) {
    if (handle_) ocr_set_preprocess_save_path(handle_, path.c_str());
  }

  /** 设置裁剪后的文本框图像保存目录（调试用），下次 detect 时保存 part_0.png, part_1.png, ... */
  void setPartImagesSavePath(const std::string& path) {
    if (handle_) ocr_set_part_imgs_save_path(handle_, path.c_str());
  }

  /** 使用 OcrDetectOptions 检测（从配置文件加载），use_crop_len=true 时用 crop_short_side_len */
  std::vector<TextBlock> detect(const cv::Mat& image, const OcrDetectOptions& opt, bool use_crop_len = false) {
    setNumThreads(opt.num_threads);
    int shortLen = use_crop_len ? opt.crop_short_side_len : opt.short_side_len;
    return detect(image, opt.padding, shortLen,
      opt.box_score_thresh, opt.box_thresh, opt.un_clip_ratio,
      opt.do_angle != 0, opt.most_angle != 0);
  }

  /**
   * 检测图像中的文字（与 OcrService 中 ocr_engine_->detect() 调用方式一致）
   * 直接传递 cv::Mat 引用，不经过原始字节拆解/重建
   */
  std::vector<TextBlock> detect(const cv::Mat& image,
    int padding = 0, int short_side_len = 960,
    float box_score_thresh = 0.6f, float box_thresh = 0.3f, float un_clip_ratio = 2.0f,
    bool do_angle = true, bool most_angle = true) {
    std::vector<TextBlock> out;
    if (!handle_ || image.empty()) return out;
    OCR_Options opt = {};
    opt.padding = padding;
    opt.short_side_len = short_side_len;
    opt.box_score_thresh = box_score_thresh;
    opt.box_thresh = box_thresh;
    opt.un_clip_ratio = un_clip_ratio;
    opt.do_angle = do_angle ? 1 : 0;
    opt.most_angle = most_angle ? 1 : 0;

    // 使用直接 cv::Mat 接口（与 OcrService 完全一致的调用路径）
    std::vector<OCR_TextBlockCpp> cpp_blocks;
    int n = ocr_detect_mat(handle_, image, &opt, cpp_blocks);
    if (n <= 0) return out;
    out.reserve(n);
    for (int i = 0; i < n; i++) {
      TextBlock b;
      for (int k = 0; k < 4; k++) {
        b.box[k][0] = cpp_blocks[i].box[k][0];
        b.box[k][1] = cpp_blocks[i].box[k][1];
      }
      b.box_score = cpp_blocks[i].box_score;
      b.text = std::move(cpp_blocks[i].text);
      b.confidence = cpp_blocks[i].confidence;
      out.push_back(std::move(b));
    }
    return out;
  }

private:
  OCR_Handle handle_ = nullptr;
};

} // namespace ocrdetect

#endif /* OCRDETECT_OCR_ENGINE_HPP */
