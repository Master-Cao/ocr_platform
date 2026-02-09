#ifndef __OCR_LITE_H__
#define __OCR_LITE_H__

#include "opencv2/core.hpp"
#include <onnxruntime/core/session/onnxruntime_cxx_api.h>
#include "OcrStruct.h"
#include "DbNet.h"
#include "AngleNet.h"
#include "CrnnNet.h"

class OcrLite {
public:
    OcrLite();

    ~OcrLite();

    void setNumThread(int numOfThread);

    void initLogger(bool isConsole, bool isPartImg, bool isResultImg);

    void enableResultTxt(const char *path, const char *imgName);
    
    /**
     * @brief 启用/禁用图像预处理增强
     * @param enable true 启用预处理（默认），false 禁用
     */
    void setPreprocess(bool enable) { enablePreprocess_ = enable; }
    
    /**
     * @brief 设置预处理图像保存路径（用于调试）
     * @param path 保存路径，空字符串表示不保存
     */
    void setPreprocessSavePath(const std::string& path);

    /**
     * @brief 设置裁剪后的文本框图像保存目录（用于调试）
     * 若为非空，detect 时会将每个裁剪块保存为该目录下的 part_0.png, part_1.png, ...
     * @param path 目录路径，空字符串表示不保存
     */
    void setPartImagesSavePath(const std::string& path);

    bool initModels(const std::string &detPath, const std::string &clsPath,
                    const std::string &recPath, const std::string &keysPath);

    void Logger(const char *format, ...);

    OcrResult detect(const char *path, const char *imgName,
                     int padding, int shortSideLen,
                     float boxScoreThresh, float boxThresh, float unClipRatio, bool doAngle, bool mostAngle);

    OcrResult detect(const cv::Mat &mat,
                     int padding, int shortSideLen,
                     float boxScoreThresh, float boxThresh, float unClipRatio, bool doAngle, bool mostAngle);

private:
    bool enablePreprocess_ = false;  // 默认禁用: Python OnnxOCR 无预处理，CLAHE 会影响 rec 模型识别
    bool isOutputConsole = false;
    bool isOutputPartImg = false;
    bool isOutputResultTxt = false;
    bool isOutputResultImg = false;
    FILE *resultTxt;
    DbNet dbNet;
    AngleNet angleNet;
    CrnnNet crnnNet;

    std::vector<cv::Mat> getPartImages(cv::Mat &src, std::vector<TextBox> &textBoxes,
                                       const char *path, const char *imgName);

    OcrResult detect(const char *path, const char *imgName,
                     cv::Mat &src, cv::Rect &originRect, ScaleParam &scale,
                     float boxScoreThresh = 0.6f, float boxThresh = 0.3f,
                     float unClipRatio = 2.0f, bool doAngle = true, bool mostAngle = true);
};

#endif //__OCR_LITE_H__
