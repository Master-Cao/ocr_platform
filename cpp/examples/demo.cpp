/**
 * @file demo.cpp
 * @brief 模板匹配 + OCR 联合示例
 *
 * 用法: demo [模型目录] [待检测图片] [模板图片]
 * 示例: demo ../OcrDetect/models test.png
 *       demo ../OcrDetect/models scene.png template.png
 */
#include <templatematch/Matcher.hpp>
#include <templatematch/ConfigLoader.hpp>
#include <ocrdetect/OcrEngine.hpp>
#include <ocrdetect/ConfigLoader.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

static void runOcrDemo(const std::string& modelsDir, const std::string& imagePath, const ocrdetect::OcrDetectOptions& ocrOpt) {
  std::cout << "\n========== OCR 检测示例 ==========\n";
  std::cout << "模型目录: " << modelsDir << "  图片: " << imagePath << "\n";

  cv::Mat img = cv::imread(imagePath);
  if (img.empty()) {
    std::cerr << "无法读取图片: " << imagePath << "\n";
    return;
  }

  ocrdetect::OcrEngine engine(modelsDir);
  if (!engine.valid()) {
    std::cerr << "OCR 引擎初始化失败，请检查模型目录: " << modelsDir << "\n";
    return;
  }
  engine.setNumThreads(ocrOpt.num_threads);

  fs::path pPre(imagePath);
  std::string preprocessPath = (pPre.parent_path() / (pPre.stem().string() + "_preprocess" + pPre.extension().string())).string();
  engine.setPreprocessSavePath(preprocessPath);
  std::string cropsDir = (pPre.parent_path() / (pPre.stem().string() + "_crops")).string();
  fs::create_directories(cropsDir);
  engine.setPartImagesSavePath(cropsDir);

  auto blocks = engine.detect(img, ocrOpt);
  std::cout << "检测到 " << blocks.size() << " 个文本框\n";
  std::string fullText;

  cv::Mat visImg = img.clone();
  for (size_t i = 0; i < blocks.size(); i++) {
    const auto& b = blocks[i];
    std::cout << "  [" << (i + 1) << "] \"" << b.text << "\" (置信度: " << b.confidence << ")\n";
    if (!b.text.empty()) {
      if (!fullText.empty()) fullText += " ";
      fullText += b.text;
    }
    std::vector<cv::Point> pts;
    for (int k = 0; k < 4; k++)
      pts.push_back(cv::Point(static_cast<int>(b.box[k][0]), static_cast<int>(b.box[k][1])));
    cv::polylines(visImg, pts, true, cv::Scalar(0, 255, 0), 2);
    cv::putText(visImg, b.text.empty() ? "?" : b.text, pts[0],
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
  }

  fs::path p(imagePath);
  std::string outPath = (p.parent_path() / (p.stem().string() + "_ocr_result" + p.extension().string())).string();
  if (cv::imwrite(outPath, visImg))
    std::cout << "已保存标注图: " << outPath << "\n";
  else
    std::cerr << "保存标注图失败: " << outPath << "\n";

  if (!fullText.empty())
    std::cout << "[OCR] 识别结果: " << fullText << "\n";
  else if (blocks.empty())
    std::cout << "[OCR] 未检测到文字\n";
  else
    std::cout << "[OCR] 检测到文本框但识别结果为空\n";
  std::cout << "========== OCR 示例结束 ==========\n";
}

static void runTemplateMatchDemo(const std::string& imagePath, const std::string& templatePath, const templatematch::Params& tmParams) {
  std::cout << "\n========== 模板匹配示例 ==========\n";
  std::cout << "场景图: " << imagePath << "  模板图: " << templatePath << "\n";

  cv::Mat scene = cv::imread(imagePath);
  cv::Mat templ = cv::imread(templatePath);
  if (scene.empty() || templ.empty()) {
    std::cerr << "无法读取图片\n";
    return;
  }

  templatematch::Matcher matcher(tmParams);
  if (!matcher.setTemplateFromFile(templatePath)) {
    std::cerr << "设置模板失败\n";
    return;
  }

  auto results = matcher.match(scene);
  std::cout << "匹配到 " << results.size() << " 个目标\n";
  for (size_t i = 0; i < results.size(); i++) {
    const auto& r = results[i];
    std::cout << "  [" << (i + 1) << "] 中心=(" << r.center_x << "," << r.center_y << ") 角度=" << r.angle << " 得分=" << r.score << "\n";
  }
  std::cout << "========== 模板匹配示例结束 ==========\n";
}

static void runCombinedDemo(const std::string& modelsDir, const std::string& imagePath,
                           const std::string& templatePath,
                           const templatematch::Params& tmParams, const ocrdetect::OcrDetectOptions& ocrOpt) {
  std::cout << "\n========== 组合示例：先匹配再 OCR ==========\n";
  cv::Mat scene = cv::imread(imagePath);
  cv::Mat templ = cv::imread(templatePath);
  if (scene.empty() || templ.empty()) {
    std::cerr << "无法读取图片\n";
    return;
  }

  templatematch::Matcher matcher(tmParams);
  if (!matcher.setTemplateFromFile(templatePath)) {
    std::cerr << "设置模板失败\n";
    return;
  }
  auto matches = matcher.match(scene);
  if (matches.empty()) {
    std::cout << "未匹配到目标，跳过裁剪 OCR\n";
    return;
  }

  ocrdetect::OcrEngine engine(modelsDir);
  if (!engine.valid()) {
    std::cerr << "OCR 引擎初始化失败\n";
    return;
  }
  engine.setNumThreads(ocrOpt.num_threads);

  const auto& r = matches[0];
  std::vector<cv::Point2f> pts = {
    cv::Point2f(static_cast<float>(r.left_top_x), static_cast<float>(r.left_top_y)),
    cv::Point2f(static_cast<float>(r.right_top_x), static_cast<float>(r.right_top_y)),
    cv::Point2f(static_cast<float>(r.right_bottom_x), static_cast<float>(r.right_bottom_y)),
    cv::Point2f(static_cast<float>(r.left_bottom_x), static_cast<float>(r.left_bottom_y))
  };
  cv::Rect roiRect = cv::boundingRect(pts);
  if (roiRect.x < 0 || roiRect.y < 0 || roiRect.x + roiRect.width > scene.cols || roiRect.y + roiRect.height > scene.rows) {
    std::cout << "匹配区域越界，跳过裁剪\n";
    return;
  }
  cv::Mat cropped = scene(roiRect).clone();
  fs::path pComb(imagePath);
  std::string cropPath = (pComb.parent_path() / (pComb.stem().string() + "_combined_crop" + pComb.extension().string())).string();
  if (cv::imwrite(cropPath, cropped))
    std::cout << "已保存组合裁剪图: " << cropPath << "\n";
  std::string preprocessCombPath = (pComb.parent_path() / (pComb.stem().string() + "_combined_preprocess" + pComb.extension().string())).string();
  engine.setPreprocessSavePath(preprocessCombPath);
  auto blocks = engine.detect(cropped, ocrOpt, true);
  std::cout << "首个匹配区域识别到 " << blocks.size() << " 行文字\n";

  cv::Mat visCropped = cropped.clone();
  std::string combined;
  for (size_t i = 0; i < blocks.size(); i++) {
    const auto& b = blocks[i];
    std::cout << "  [" << (i + 1) << "] \"" << b.text << "\"\n";
    if (!b.text.empty()) { if (!combined.empty()) combined += " "; combined += b.text; }
    std::vector<cv::Point> pts;
    for (int k = 0; k < 4; k++)
      pts.push_back(cv::Point(static_cast<int>(b.box[k][0]), static_cast<int>(b.box[k][1])));
    cv::polylines(visCropped, pts, true, cv::Scalar(0, 255, 0), 2);
    cv::putText(visCropped, b.text.empty() ? "?" : b.text, pts[0],
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
  }
  if (!combined.empty()) std::cout << "完整识别结果: " << combined << "\n";

  fs::path p(imagePath);
  std::string outPath = (p.parent_path() / (p.stem().string() + "_combined_ocr_result" + p.extension().string())).string();
  if (cv::imwrite(outPath, visCropped))
    std::cout << "已保存组合标注图: " << outPath << "\n";
  std::cout << "========== 组合示例结束 ==========\n";
}

static void printUsage(const char* prog) {
  std::cout << "用法: " << prog << " [模型目录] [待检测图片] [模板图片] [--config-dir <目录>]\n"
            << "  模型目录: 含 det.onnx/cls.onnx/rec.onnx/keys，默认 ../OcrDetect/models\n"
            << "  待检测图片: 必填\n"
            << "  模板图片: 可选，提供则运行匹配+组合示例\n"
            << "  --config-dir: 配置文件目录，默认 ./config\n\n"
            << "示例:\n  " << prog << " ../OcrDetect/models test.png\n"
            << "  " << prog << " ../OcrDetect/models scene.png template.png\n";
}

int main(int argc, char* argv[]) {
  std::string modelsDir = "../OcrDetect/models";
  std::string imagePath;
  std::string templatePath;
  std::string configDir = "config";
  std::vector<std::string> posArgs;
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--config-dir" && i + 1 < argc) {
      configDir = argv[++i];
    } else {
      posArgs.push_back(argv[i]);
    }
  }
  if (posArgs.size() >= 1) modelsDir = posArgs[0];
  if (posArgs.size() >= 2) imagePath = posArgs[1];
  if (posArgs.size() >= 3) templatePath = posArgs[2];

  std::vector<std::string> tmPaths = { configDir + "/templatematch.conf", "config/templatematch.conf", "../config/templatematch.conf" };
  std::vector<std::string> ocrPaths = { configDir + "/ocrdetect.conf", "config/ocrdetect.conf", "../config/ocrdetect.conf" };
  std::string tmLoaded, ocrLoaded;
  auto tmParams = templatematch::loadParamsFromFileWithFallback(tmPaths, &tmLoaded);
  auto ocrOpt = ocrdetect::loadOcrDetectConfigFromFileWithFallback(ocrPaths, &ocrLoaded);

  if (imagePath.empty()) {
    printUsage(argv[0]);
    return 0;
  }
  if (!templatePath.empty()) {
    runTemplateMatchDemo(imagePath, templatePath, tmParams);
    runCombinedDemo(modelsDir, imagePath, templatePath, tmParams, ocrOpt);
  }
  runOcrDemo(modelsDir, imagePath, ocrOpt);
  return 0;
}
