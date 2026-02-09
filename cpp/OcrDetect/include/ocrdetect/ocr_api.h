/**
 * @file ocr_api.h
 * @brief OCR 检测 C API，便于跨语言与跨编译器使用（.so / .dll）
 */
#ifndef OCRDETECT_OCR_API_H
#define OCRDETECT_OCR_API_H

#include "export.h"

#ifdef __cplusplus
#include <opencv2/core.hpp>
#include <vector>
#include <string>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* OCR_Handle;

/** 单点 (x,y) */
typedef struct OCR_Point { double x, y; } OCR_Point;

/** 检测选项 */
typedef struct OCR_Options {
  int padding;           /**< 外扩白边，默认 0 */
  int short_side_len;    /**< 短边缩放目标，0 表示不缩放，默认 960 */
  float box_score_thresh;
  float box_thresh;
  float un_clip_ratio;
  int do_angle;          /**< 1 启用方向分类 */
  int most_angle;
} OCR_Options;

/** 单个文本框结果（4 个顶点 + 文本 + 置信度） */
typedef struct OCR_TextBlock {
  OCR_Point box[4];     /**< 左上、右上、右下、左下 */
  float box_score;
  char* text;           /**< 识别文本，由库分配，需用 ocr_free_text 释放 */
  float confidence;
} OCR_TextBlock;

/** 检测结果（调用方分配 OCR_TextBlock 数组，库填充；或使用 ocr_detect_alloc 返回需 ocr_free_result 释放） */
typedef struct OCR_Result {
  int count;
  OCR_TextBlock* blocks;
  double detect_time_ms;
} OCR_Result;

/**
 * 创建 OCR 引擎并直接加载模型
 * @param models_dir 模型目录路径（UTF-8），内含 det.onnx、cls.onnx、rec.onnx、ppocr_keys_v1.txt 或 keys.txt
 * @return 句柄，失败返回 NULL
 */
OCRDETECT_OCR_API OCR_Handle OCRDETECT_OCR_CALL ocr_create(const char* models_dir);

/**
 * 销毁引擎
 */
OCRDETECT_OCR_API void OCRDETECT_OCR_CALL ocr_destroy(OCR_Handle h);

/**
 * 设置线程数
 */
OCRDETECT_OCR_API void OCRDETECT_OCR_CALL ocr_set_num_threads(OCR_Handle h, int n);

/**
 * 设置预处理图像保存路径（调试用）
 * 下次 detect 时，若启用预处理，会将预处理后的图像保存到该路径
 * @param path 保存路径，NULL 或空字符串表示不保存
 */
OCRDETECT_OCR_API void OCRDETECT_OCR_CALL ocr_set_preprocess_save_path(OCR_Handle h, const char* path);

/**
 * 设置裁剪后的文本框图像保存目录（调试用）
 * 下次 detect 时，会将每个裁剪块保存为该目录下的 part_0.png, part_1.png, ...
 * @param path 目录路径，NULL 或空字符串表示不保存
 */
OCRDETECT_OCR_API void OCRDETECT_OCR_CALL ocr_set_part_imgs_save_path(OCR_Handle h, const char* path);

/**
 * 从内存图像检测
 * @param image_data BGR 或灰度数据（行优先）
 * @param width height channels (3 或 1)
 * @param options 可为 NULL，使用默认
 * @param results 输出数组，调用方分配，至少 max_results 个；每个 block 的 text 由库分配
 * @param max_results 最多返回块数
 * @return 实际数量 (>=0)，<0 错误
 */
OCRDETECT_OCR_API int OCRDETECT_OCR_CALL ocr_detect(
  OCR_Handle h,
  const unsigned char* image_data, int width, int height, int channels,
  const OCR_Options* options,
  OCR_TextBlock* results, int max_results);

/**
 * 释放某次检测中由库分配的 text 指针（对 results[i].text 逐个调用或整批释放）
 */
OCRDETECT_OCR_API void OCRDETECT_OCR_CALL ocr_free_text(char* text);

#ifdef __cplusplus
}
#endif

/* ============================================================
 * C++ 直接接口 —— 与 OcrService 完全一致的调用方式
 * 直接传递 cv::Mat，不经过原始字节拷贝
 * ============================================================ */
#ifdef __cplusplus

/** 单个文本块结果（C++ 版本） */
struct OCR_TextBlockCpp {
  float box[4][2];     /**< 4 个顶点 (x,y): 左上、右上、右下、左下 */
  float box_score;
  std::string text;    /**< 识别文本 */
  float confidence;
};

/**
 * 直接从 cv::Mat 检测（与 OcrService 中 ocr_engine_->detect() 调用方式一致）
 * @param h       ocr_create 返回的句柄
 * @param image   BGR 格式的 cv::Mat（直接传引用，零拷贝）
 * @param options 可为 NULL，使用默认参数
 * @param out     输出的文本块列表
 * @return        检测到的文本块数量，<0 表示错误
 */
OCRDETECT_OCR_API int OCRDETECT_OCR_CALL ocr_detect_mat(
  OCR_Handle h,
  const cv::Mat& image,
  const OCR_Options* options,
  std::vector<OCR_TextBlockCpp>& out);

#endif /* __cplusplus */

#endif /* OCRDETECT_OCR_API_H */
