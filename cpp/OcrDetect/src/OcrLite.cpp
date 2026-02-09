#include "OcrLite.h"
#include "OcrUtils.h"
#include <stdarg.h> //windows&linux
#include <string>

// 预处理图像保存路径（全局变量用于调试）
static std::string g_preprocessSavePath = "";
// 裁剪后的文本框图像保存目录（非空则保存 part_0.png, part_1.png, ...）
static std::string g_partImagesSavePath = "";

OcrLite::OcrLite() : enablePreprocess_(false) {}
// 注意：enablePreprocess_ 默认禁用，因为 Python OnnxOCR 不做任何预处理
// CLAHE + 锐化会改变像素分布，导致 rec 模型无法正确识别
// 如需启用预处理，请调用 setPreprocess(true)

void OcrLite::setPreprocessSavePath(const std::string& path) {
    g_preprocessSavePath = path;
}

void OcrLite::setPartImagesSavePath(const std::string& path) {
    g_partImagesSavePath = path;
}

/**
 * @brief 图像预处理增强，提高文字检测效果
 * 
 * 针对低对比度、浅色文字等情况进行增强处理
 */
cv::Mat preprocessImage(const cv::Mat& src) {
    cv::Mat result = src.clone();
    cv::Mat lab;
    cv::cvtColor(result, lab, cv::COLOR_RGB2Lab);
    std::vector<cv::Mat> labChannels;
    cv::split(lab, labChannels);
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
    clahe->setClipLimit(2.0);
    clahe->setTilesGridSize(cv::Size(8, 8));
    clahe->apply(labChannels[0], labChannels[0]);
    cv::merge(labChannels, lab);
    cv::cvtColor(lab, result, cv::COLOR_Lab2RGB);
    cv::Mat sharpened;
    cv::Mat kernel = (cv::Mat_<float>(3, 3) << 0, -1, 0, -1, 5, -1, 0, -1, 0);
    cv::filter2D(result, sharpened, -1, kernel);
    cv::addWeighted(result, 0.5, sharpened, 0.5, 0, result);
    if (!g_preprocessSavePath.empty()) {
        cv::Mat bgr;
        cv::cvtColor(result, bgr, cv::COLOR_RGB2BGR);
        if (!cv::imwrite(g_preprocessSavePath, bgr))
            fprintf(stderr, "预处理图保存失败: %s\n", g_preprocessSavePath.c_str());
    }
    return result;
}

OcrLite::~OcrLite() {
    if (isOutputResultTxt) {
        fclose(resultTxt);
    }
}

void OcrLite::setNumThread(int numOfThread) {
    dbNet.setNumThread(numOfThread);
    angleNet.setNumThread(numOfThread);
    crnnNet.setNumThread(numOfThread);
}

void OcrLite::initLogger(bool isConsole, bool isPartImg, bool isResultImg) {
    isOutputConsole = isConsole;
    isOutputPartImg = isPartImg;
    isOutputResultImg = isResultImg;
}

void OcrLite::enableResultTxt(const char *path, const char *imgName) {
    isOutputResultTxt = true;
    std::string resultTxtPath = getResultTxtFilePath(path, imgName);
    resultTxt = fopen(resultTxtPath.c_str(), "w");
}

bool OcrLite::initModels(const std::string &detPath, const std::string &clsPath,
                         const std::string &recPath, const std::string &keysPath) {
    Logger("=====Init Models=====\n");
    Logger("--- Init DbNet ---\n");
    dbNet.initModel(detPath);

    Logger("--- Init AngleNet ---\n");
    angleNet.initModel(clsPath);

    Logger("--- Init CrnnNet ---\n");
    crnnNet.initModel(recPath, keysPath);

    Logger("Init Models Success!\n");
    return true;
}

void OcrLite::Logger(const char *format, ...) {
    if (!(isOutputConsole || isOutputResultTxt)) return;
    char *buffer = (char *) malloc(8192);
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);
    if (isOutputConsole) printf("%s", buffer);
    if (isOutputResultTxt) fprintf(resultTxt, "%s", buffer);
    free(buffer);
}

cv::Mat makePadding(cv::Mat &src, const int padding) {
    if (padding <= 0) return src;
    cv::Scalar paddingScalar = {255, 255, 255};
    cv::Mat paddingSrc;
    cv::copyMakeBorder(src, paddingSrc, padding, padding, padding, padding, cv::BORDER_ISOLATED, paddingScalar);
    return paddingSrc;
}

OcrResult OcrLite::detect(const char *path, const char *imgName,
                          const int padding, const int shortSideLen,
                          float boxScoreThresh, float boxThresh, float unClipRatio, bool doAngle, bool mostAngle) {
    std::string imgFile = getSrcImgFilePath(path, imgName);
    cv::Mat bgrSrc = imread(imgFile, cv::IMREAD_COLOR);
    if (bgrSrc.empty()) {
        fprintf(stderr, "无法加载图像: %s\n", imgFile.c_str());
        return OcrResult{};
    }
    cv::Mat originSrc;
    cvtColor(bgrSrc, originSrc, cv::COLOR_BGR2RGB);
    int originMinSide = (std::min)(originSrc.cols, originSrc.rows);
    int originMaxSide = (std::max)(originSrc.cols, originSrc.rows);
    int resize;
    if (shortSideLen <= 0 || shortSideLen >= originMinSide) {
        resize = originMaxSide;
    } else {
        float scale = (float)shortSideLen / (float)originMinSide;
        resize = (int)(originMaxSide * scale);
        resize = 32 * (resize / 32);
        if (resize < 32) resize = 32;
    }
    resize += 2*padding;
    cv::Rect paddingRect(padding, padding, originSrc.cols, originSrc.rows);
    cv::Mat paddingSrc = makePadding(originSrc, padding);
    ScaleParam scale = getScaleParam(paddingSrc, resize);
    return detect(path, imgName, paddingSrc, paddingRect, scale,
                  boxScoreThresh, boxThresh, unClipRatio, doAngle, mostAngle);
}

OcrResult OcrLite::detect(const cv::Mat& mat, int padding, int shortSideLen, float boxScoreThresh, float boxThresh, float unClipRatio, bool doAngle, bool mostAngle)
{
    if (mat.empty()) {
        fprintf(stderr, "输入图像为空\n");
        return OcrResult{};
    }
    cv::Mat originSrc;
    cvtColor(mat, originSrc, cv::COLOR_BGR2RGB);
    if (enablePreprocess_)
        originSrc = preprocessImage(originSrc);
    int originMinSide = (std::min)(originSrc.cols, originSrc.rows);
    int originMaxSide = (std::max)(originSrc.cols, originSrc.rows);
    int resize;
    if (shortSideLen <= 0 || shortSideLen >= originMinSide) {
        resize = originMaxSide;
    } else {
        float scale = (float)shortSideLen / (float)originMinSide;
        resize = (int)(originMaxSide * scale);
        resize = 32 * (resize / 32);
        if (resize < 32) resize = 32;
    }
    resize += 2 * padding;
    cv::Rect paddingRect(padding, padding, originSrc.cols, originSrc.rows);
    cv::Mat paddingSrc = makePadding(originSrc, padding);
    ScaleParam scale = getScaleParam(paddingSrc, resize);
    return detect(NULL, NULL, paddingSrc, paddingRect, scale,
        boxScoreThresh, boxThresh, unClipRatio, doAngle, mostAngle);
}

std::vector<cv::Mat> OcrLite::getPartImages(cv::Mat &src, std::vector<TextBox> &textBoxes,
                                            const char *path, const char *imgName) {
    std::vector<cv::Mat> partImages;
    for (size_t i = 0; i < textBoxes.size(); ++i) {
        cv::Mat partImg = getRotateCropImage(src, textBoxes[i].boxPoint);
        if (partImg.empty())
            fprintf(stderr, "文本框[%zu] 提取为空\n", i);
        partImages.emplace_back(partImg);
        if (!g_partImagesSavePath.empty()) {
            std::string cropPath = g_partImagesSavePath + "/part_" + std::to_string(i) + ".png";
            saveImg(partImg, cropPath.c_str());
        }
        if (isOutputPartImg && path != NULL && imgName != NULL) {
            std::string debugImgFile = getDebugImgFilePath(path, imgName, (int)i, "-part-");
            saveImg(partImg, debugImgFile.c_str());
        }
    }
    return partImages;
}

OcrResult OcrLite::detect(const char *path, const char *imgName,
                          cv::Mat &src, cv::Rect &originRect, ScaleParam &scale,
                          float boxScoreThresh, float boxThresh, float unClipRatio, bool doAngle, bool mostAngle) {

    cv::Mat textBoxPaddingImg = src.clone();
    int thickness = getThickness(src);

    Logger("=====Start detect=====\n");
    Logger("ScaleParam(sw:%d,sh:%d,dw:%d,dh:%d,%f,%f)\n", scale.srcWidth, scale.srcHeight,
           scale.dstWidth, scale.dstHeight,
           scale.ratioWidth, scale.ratioHeight);

    Logger("---------- step: dbNet getTextBoxes ----------\n");
    double startTime = getCurrentTime();
    std::vector<TextBox> textBoxes = dbNet.getTextBoxes(src, scale, boxScoreThresh, boxThresh, unClipRatio);
    double endDbNetTime = getCurrentTime();
    double dbNetTime = endDbNetTime - startTime;
    Logger("dbNetTime(%fms)\n", dbNetTime);

    for (int i = 0; i < textBoxes.size(); ++i) {
        Logger("TextBox[%d](+padding)[score(%f),[x: %d, y: %d], [x: %d, y: %d], [x: %d, y: %d], [x: %d, y: %d]]\n", i,
               textBoxes[i].score,
               textBoxes[i].boxPoint[0].x, textBoxes[i].boxPoint[0].y,
               textBoxes[i].boxPoint[1].x, textBoxes[i].boxPoint[1].y,
               textBoxes[i].boxPoint[2].x, textBoxes[i].boxPoint[2].y,
               textBoxes[i].boxPoint[3].x, textBoxes[i].boxPoint[3].y);
    }

    Logger("---------- step: drawTextBoxes ----------\n");
    drawTextBoxes(textBoxPaddingImg, textBoxes, thickness);

    //---------- getPartImages ----------
    std::vector<cv::Mat> partImages = getPartImages(src, textBoxes, path, imgName);

    Logger("---------- step: angleNet getAngles ----------\n");
    std::vector<Angle> angles;
    angles = angleNet.getAngles(partImages, path, imgName, doAngle, mostAngle);

    //Log Angles
    for (int i = 0; i < angles.size(); ++i) {
        Logger("angle[%d][index(%d), score(%f), time(%fms)]\n", i, angles[i].index, angles[i].score, angles[i].time);
    }

    //Rotate partImgs：与 OnnxOCR 一致，label_list=['0','180'] 即 index 0=0°、index 1=180°
    // 仅当分类为 180°（index==1）时旋转，否则会误把正放的文字旋转成倒的导致 rec 无法识别
    for (int i = 0; i < partImages.size(); ++i) {
        if (angles[i].index == 1) {
            partImages.at(i) = matRotateClockWise180(partImages[i]);
        }
    }

    // cpp_projects/OcrLiteOnnx 不进行 RGB->BGR 转换，rec 模型接收 RGB
    // Python OnnxOCR 使用 BGR（cv2.imread 直接裁剪），但 PPOCRv4 SVTR_LCNet 可能训练时用 RGB
    // 实测：传递 RGB 给 rec 模型（与 OcrLiteOnnx 一致）可正确识别
    Logger("---------- step: crnnNet getTextLine ----------\n");
    std::vector<TextLine> textLines = crnnNet.getTextLines(partImages, path, imgName);
    //Log TextLines
    for (int i = 0; i < textLines.size(); ++i) {
        Logger("textLine[%d](%s)\n", i, textLines[i].text.c_str());
        std::ostringstream txtScores;
        for (int s = 0; s < textLines[i].charScores.size(); ++s) {
            if (s == 0) {
                txtScores << textLines[i].charScores[s];
            } else {
                txtScores << " ," << textLines[i].charScores[s];
            }
        }
        Logger("textScores[%d]{%s}\n", i, std::string(txtScores.str()).c_str());
        Logger("crnnTime[%d](%fms)\n", i, textLines[i].time);
    }

    std::vector<TextBlock> textBlocks;
    for (int i = 0; i < textLines.size(); ++i) {
        std::vector<cv::Point> boxPoint = std::vector<cv::Point>(4);
        int padding = originRect.x;//padding conversion
        boxPoint[0] = cv::Point(textBoxes[i].boxPoint[0].x - padding, textBoxes[i].boxPoint[0].y - padding);
        boxPoint[1] = cv::Point(textBoxes[i].boxPoint[1].x - padding, textBoxes[i].boxPoint[1].y - padding);
        boxPoint[2] = cv::Point(textBoxes[i].boxPoint[2].x - padding, textBoxes[i].boxPoint[2].y - padding);
        boxPoint[3] = cv::Point(textBoxes[i].boxPoint[3].x - padding, textBoxes[i].boxPoint[3].y - padding);
        TextBlock textBlock{boxPoint, textBoxes[i].score, angles[i].index, angles[i].score,
                            angles[i].time, textLines[i].text, textLines[i].charScores, textLines[i].time,
                            angles[i].time + textLines[i].time};
        textBlocks.emplace_back(textBlock);
    }

    double endTime = getCurrentTime();
    double fullTime = endTime - startTime;
    Logger("=====End detect=====\n");
    Logger("FullDetectTime(%fms)\n", fullTime);

    //cropped to original size
    cv::Mat rgbBoxImg, textBoxImg;

    if (originRect.x > 0 && originRect.y > 0) {
        textBoxPaddingImg(originRect).copyTo(rgbBoxImg);
    } else {
        rgbBoxImg = textBoxPaddingImg;
    }
    cvtColor(rgbBoxImg, textBoxImg, cv::COLOR_RGB2BGR);//convert to BGR for Output Result Img

    //Save result.jpg
    if (isOutputResultImg) {
        std::string resultImgFile = getResultImgFilePath(path, imgName);
        imwrite(resultImgFile, textBoxImg);
    }

    std::string strRes;
    for (int i = 0; i < textBlocks.size(); ++i) {
        strRes.append(textBlocks[i].text);
        strRes.append("\n");
    }

    return OcrResult{dbNetTime, textBlocks, textBoxImg, fullTime, strRes};
}