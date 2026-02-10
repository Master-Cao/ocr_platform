/**
 * @file demo_tm_ocr.cpp
 * @brief 模板匹配 + OCR 串联示例：先模板匹配，再对匹配区域做 OCR 识别
 *
 * 用法: demo_tm_ocr <模型目录> <场景图> <模板图> [输出图] [--config-dir <目录>]
 * 配置: 从 config/ 或 --config-dir 读取 templatematch.conf、ocrdetect.conf
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
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

static void printUsage(const char* prog) {
  std::cout << "用法: " << (prog ? prog : "demo_tm_ocr")
            << " <模型目录> <场景图> <模板图> [输出图] [--config-dir <目录>]\n"
            << "  模型目录: 含 det.onnx/cls.onnx/rec.onnx/ppocr_keys_v1.txt\n"
            << "  场景图:   待检测的完整图像\n"
            << "  模板图:   待匹配的模板图像\n"
            << "  输出图:   可选，保存标注后的结果图\n"
            << "  --config-dir: 可选，配置文件目录，默认 ./config\n\n"
            << "配置文件: templatematch.conf、ocrdetect.conf，修改后无需重新编译\n\n"
            << "示例:\n  " << (prog ? prog : "demo_tm_ocr")
            << " ./OcrDetect/models scene.png template.png result.png\n";
}

int main(int argc, char* argv[]) {
  if (argc < 4) {
    printUsage(argv[0]);
    return argc < 2 ? 0 : -1;
  }

  std::string modelsDir = argv[1];
  std::string scenePath = argv[2];
  std::string templatePath = argv[3];
  std::string outputPath;
  std::string configDir = "config";

  for (int i = 4; i < argc; i++) {
    if (std::string(argv[i]) == "--config-dir" && i + 1 < argc) {
      configDir = argv[++i];
    } else if (outputPath.empty()) {
      outputPath = argv[i];
    }
  }

  // 加载配置（尝试多路径，文件不存在时使用默认值）
  std::vector<std::string> tmPaths = {
    configDir + "/templatematch.conf",
    "config/templatematch.conf",
    "../config/templatematch.conf",
    "templatematch/config/templatematch.conf"
  };
  std::string tmLoadedPath;
  auto tmParams = templatematch::loadParamsFromFileWithFallback(tmPaths, &tmLoadedPath);
  std::cout << "[配置] templatematch: " << tmLoadedPath << " (angle=" << tmParams.angle << ")\n";

  std::vector<std::string> ocrPaths = {
    configDir + "/ocrdetect.conf",
    "config/ocrdetect.conf",
    "../config/ocrdetect.conf",
    "OcrDetect/config/ocrdetect.conf"
  };
  std::string ocrLoadedPath;
  auto ocrOpt = ocrdetect::loadOcrDetectConfigFromFileWithFallback(ocrPaths, &ocrLoadedPath);

  std::cout << "========== 模板匹配 + OCR 串联示例 ==========\n";
  std::cout << "模型目录: " << modelsDir << "\n";
  std::cout << "场景图:   " << scenePath << "\n";
  std::cout << "模板图:   " << templatePath << "\n";

  cv::Mat scene = cv::imread(scenePath);
  if (scene.empty()) {
    std::cerr << "错误: 无法读取场景图 " << scenePath << "\n";
    return -1;
  }

  cv::Mat templ = cv::imread(templatePath);
  if (templ.empty()) {
    std::cerr << "错误: 无法读取模板图 " << templatePath << "\n";
    return -1;
  }

  // Step 1: 模板匹配（使用配置文件参数）
  std::cout << "\n--- Step 1: 模板匹配 ---\n";
  templatematch::Matcher matcher(tmParams);
  if (!matcher.valid()) {
    std::cerr << "错误: 匹配器创建失败\n";
    return -1;
  }
  if (!matcher.setTemplateFromFile(templatePath)) {
    std::cerr << "错误: 设置模板失败\n";
    return -1;
  }

  auto t0 = std::chrono::high_resolution_clock::now();
  auto matches = matcher.match(scene);
  auto t1 = std::chrono::high_resolution_clock::now();
  double tmMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
  std::cout << "模板匹配耗时: " << tmMs << " ms\n";
  std::cout << "匹配到 " << matches.size() << " 个目标\n";

  if (matches.empty()) {
    std::cout << "未匹配到目标，退出\n";
    return 0;
  }

  for (size_t i = 0; i < matches.size(); i++) {
    const auto& r = matches[i];
    std::cout << "  [" << (i + 1) << "] 中心=(" << r.center_x << ", " << r.center_y
              << ") 角度=" << r.angle << " 得分=" << r.score << "\n";
  }

  // Step 2: OCR 引擎
  std::cout << "\n--- Step 2: OCR 识别 ---\n";
  ocrdetect::OcrEngine engine(modelsDir);
  if (!engine.valid()) {
    std::cerr << "错误: OCR 引擎初始化失败，请检查模型目录: " << modelsDir << "\n";
    return -1;
  }
  engine.setNumThreads(ocrOpt.num_threads);

  cv::Mat visImg = scene.clone();
  std::string fullText;
  double totalOcrMs = 0;

  for (size_t idx = 0; idx < matches.size(); idx++) {
    const auto& r = matches[idx];
    std::vector<cv::Point2f> pts = {
        cv::Point2f(static_cast<float>(r.left_top_x), static_cast<float>(r.left_top_y)),
        cv::Point2f(static_cast<float>(r.right_top_x), static_cast<float>(r.right_top_y)),
        cv::Point2f(static_cast<float>(r.right_bottom_x), static_cast<float>(r.right_bottom_y)),
        cv::Point2f(static_cast<float>(r.left_bottom_x), static_cast<float>(r.left_bottom_y))};
    cv::Rect roiRect = cv::boundingRect(pts);

    if (roiRect.x < 0 || roiRect.y < 0 ||
        roiRect.x + roiRect.width > scene.cols || roiRect.y + roiRect.height > scene.rows) {
      std::cout << "  [匹配 " << (idx + 1) << "] 区域越界，跳过 OCR\n";
      continue;
    }

    cv::Mat cropped = scene(roiRect).clone();

    auto t2 = std::chrono::high_resolution_clock::now();
    auto blocks = engine.detect(cropped, ocrOpt, true);
    auto t3 = std::chrono::high_resolution_clock::now();
    double ocrMs = std::chrono::duration<double, std::milli>(t3 - t2).count();
    totalOcrMs += ocrMs;

    std::cout << "  [匹配 " << (idx + 1) << "] OCR 耗时: " << ocrMs << " ms, 识别到 " << blocks.size() << " 个文本框\n";

    std::string regionText;
    for (size_t i = 0; i < blocks.size(); i++) {
      const auto& b = blocks[i];
      std::cout << "    [" << (i + 1) << "] \"" << b.text << "\" (置信度: " << b.confidence << ")\n";
      if (!b.text.empty()) {
        if (!regionText.empty()) regionText += " ";
        regionText += b.text;
      }
      // 在裁剪图上绘制（坐标是相对于裁剪区域的）
      std::vector<cv::Point> boxPts;
      for (int k = 0; k < 4; k++)
        boxPts.push_back(cv::Point(static_cast<int>(b.box[k][0]), static_cast<int>(b.box[k][1])));
      cv::polylines(cropped, boxPts, true, cv::Scalar(0, 255, 0), 2);
      cv::putText(cropped, b.text.empty() ? "?" : b.text, boxPts[0],
                  cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
    }

    if (!regionText.empty()) {
      if (!fullText.empty()) fullText += " | ";
      fullText += regionText;
    }

    // 在原图上绘制匹配框
    std::vector<cv::Point> matchPts = {
        cv::Point(static_cast<int>(r.left_top_x), static_cast<int>(r.left_top_y)),
        cv::Point(static_cast<int>(r.right_top_x), static_cast<int>(r.right_top_y)),
        cv::Point(static_cast<int>(r.right_bottom_x), static_cast<int>(r.right_bottom_y)),
        cv::Point(static_cast<int>(r.left_bottom_x), static_cast<int>(r.left_bottom_y))};
    cv::polylines(visImg, matchPts, true, cv::Scalar(0, 255, 0), 2);
    cv::putText(visImg, regionText.empty() ? "?" : regionText,
                matchPts[0], cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 1, cv::LINE_AA);

    // 可选：保存每个匹配区域的裁剪+OCR 结果
    if (!outputPath.empty()) {
      fs::path p(outputPath);
      std::string cropPath = (p.parent_path() / (p.stem().string() + "_crop" + std::to_string(idx) + p.extension().string())).string();
      cv::imwrite(cropPath, cropped);
    }
  }

  std::cout << "\nOCR 总耗时: " << totalOcrMs << " ms\n";
  if (!fullText.empty()) {
    std::cout << "识别结果: " << fullText << "\n";
  }

  if (!outputPath.empty()) {
    if (cv::imwrite(outputPath, visImg)) {
      std::cout << "已保存结果图: " << outputPath << "\n";
    } else {
      std::cerr << "保存结果图失败: " << outputPath << "\n";
    }
  }

  std::cout << "========== 示例结束 ==========\n";
  return 0;
}
