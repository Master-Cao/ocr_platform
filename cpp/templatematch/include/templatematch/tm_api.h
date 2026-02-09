/**
 * @file tm_api.h
 * @brief 模板匹配 C API，便于跨语言与跨编译器使用（.so / .dll）
 */
#ifndef TEMPLATEMATCH_TM_API_H
#define TEMPLATEMATCH_TM_API_H

#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 不透明句柄 */
typedef void* TM_Handle;

/** 匹配参数 */
typedef struct TM_Params {
  int max_count;           /**< 最大匹配数量，默认 200 */
  double score_threshold;  /**< 得分阈值 [0,1]，默认 0.5 */
  double iou_threshold;    /**< 重叠框 IOU 去重阈值，默认 0.0 */
  double angle;            /**< 匹配角度范围，默认 0 */
  double min_area;         /**< 顶层金字塔最小面积，默认 256 */
} TM_Params;

/** 单次匹配结果（与 C++ MatchResult 对应） */
typedef struct TM_MatchResult {
  double left_top_x, left_top_y;
  double left_bottom_x, left_bottom_y;
  double right_top_x, right_top_y;
  double right_bottom_x, right_bottom_y;
  double center_x, center_y;
  double angle;
  double score;
} TM_MatchResult;

/**
 * 创建匹配器
 * @param params 参数，可为 NULL（使用默认）
 * @return 句柄，失败返回 NULL
 */
TEMPLATEMATCH_API TM_Handle TEMPLATEMATCH_CALL tm_create(const TM_Params* params);

/**
 * 销毁匹配器
 * @param h 句柄（可为 NULL）
 */
TEMPLATEMATCH_API void TEMPLATEMATCH_CALL tm_destroy(TM_Handle h);

/**
 * 从内存设置模板（灰度图）
 * @param h 句柄
 * @param data 图像数据（行优先，灰度）
 * @param width 宽度
 * @param height 高度
 * @param channels 通道数，仅支持 1
 * @return 0 成功，<0 失败
 */
TEMPLATEMATCH_API int TEMPLATEMATCH_CALL tm_set_template_from_memory(
  TM_Handle h, const unsigned char* data, int width, int height, int channels);

/**
 * 从文件设置模板（自动转灰度）
 * @param h 句柄
 * @param file_path 文件路径（UTF-8）
 * @return 0 成功，<0 失败
 */
TEMPLATEMATCH_API int TEMPLATEMATCH_CALL tm_set_template_from_file(TM_Handle h, const char* file_path);

/**
 * 在图像中匹配
 * @param h 句柄
 * @param image_data 图像数据（行优先，灰度）
 * @param width 宽度
 * @param height 高度
 * @param channels 通道数，仅支持 1
 * @param results 输出结果数组（调用方分配，至少 max_results 个）
 * @param max_results 最多返回结果数
 * @return 匹配数量（>=0），<0 表示错误
 */
TEMPLATEMATCH_API int TEMPLATEMATCH_CALL tm_match(
  TM_Handle h,
  const unsigned char* image_data, int width, int height, int channels,
  TM_MatchResult* results, int max_results);

#ifdef __cplusplus
}
#endif

#endif /* TEMPLATEMATCH_TM_API_H */
