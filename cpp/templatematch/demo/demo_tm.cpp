/**
 * @file demo_tm.cpp
 * @brief 模板匹配测试 Demo
 *
 * 用法: demo_tm <场景图> <模板图> [输出图] [--config-dir <目录>]
 * 配置: config/templatematch.conf
 */
#include <templatematch/Matcher.hpp>
#include <templatematch/ConfigLoader.hpp>
#include <opencv2/opencv.hpp>
#include <vector>
#include <iostream>
#include <string>
#include <chrono>

int main(int argc, char* argv[]) {
  if (argc < 3) {
    std::cout << "用法: " << (argv[0] ? argv[0] : "demo_tm")
              << " <场景图> <模板图> [输出图] [--config-dir <目录>]\n"
              << "示例: demo_tm scene.png template.png\n"
              << "      demo_tm scene.png template.png result.png\n";
    return argc < 2 ? 0 : -1;
  }

  std::string scenePath = argv[1];
  std::string templatePath = argv[2];
  std::string outputPath;
  std::string configDir = "config";
  for (int i = 3; i < argc; i++) {
    if (std::string(argv[i]) == "--config-dir" && i + 1 < argc) {
      configDir = argv[++i];
    } else if (outputPath.empty()) {
      outputPath = argv[i];
    }
  }

  std::vector<std::string> tmPaths = {
    configDir + "/templatematch.conf",
    "config/templatematch.conf",
    "../config/templatematch.conf",
    "templatematch/config/templatematch.conf"
  };
  std::string tmLoadedPath;
  auto params = templatematch::loadParamsFromFileWithFallback(tmPaths, &tmLoadedPath);
  std::cout << "[配置] " << tmLoadedPath << " (angle=" << params.angle << ")\n";

  std::cout << "========== 模板匹配 Demo ==========\n";
  std::cout << "场景图: " << scenePath << "\n";
  std::cout << "模板图: " << templatePath << "\n";

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

  templatematch::Matcher matcher(params);
  if (!matcher.valid()) {
    std::cerr << "错误: 匹配器创建失败\n";
    return -1;
  }

  if (!matcher.setTemplateFromFile(templatePath)) {
    std::cerr << "错误: 设置模板失败\n";
    return -1;
  }

  auto t0 = std::chrono::high_resolution_clock::now();
  auto results = matcher.match(scene);
  auto t1 = std::chrono::high_resolution_clock::now();
  double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  std::cout << "模板匹配耗时: " << ms << " ms\n";
  std::cout << "匹配到 " << results.size() << " 个目标\n";

  cv::Mat visImg = scene.clone();
  for (size_t i = 0; i < results.size(); i++) {
    const auto& r = results[i];
    std::cout << "  [" << (i + 1) << "] 中心=(" << r.center_x << ", " << r.center_y
              << ") 角度=" << r.angle << " 得分=" << r.score << "\n";

    // 绘制匹配框（四个角点连线）
    std::vector<cv::Point> pts = {
        cv::Point(static_cast<int>(r.left_top_x), static_cast<int>(r.left_top_y)),
        cv::Point(static_cast<int>(r.right_top_x), static_cast<int>(r.right_top_y)),
        cv::Point(static_cast<int>(r.right_bottom_x), static_cast<int>(r.right_bottom_y)),
        cv::Point(static_cast<int>(r.left_bottom_x), static_cast<int>(r.left_bottom_y))};
    cv::polylines(visImg, pts, true, cv::Scalar(0, 255, 0), 2);
    cv::putText(visImg, std::to_string(i + 1),
                cv::Point(static_cast<int>(r.left_top_x), static_cast<int>(r.left_top_y) - 5),
                cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
  }

  if (!outputPath.empty()) {
    if (cv::imwrite(outputPath, visImg)) {
      std::cout << "已保存可视化结果: " << outputPath << "\n";
    } else {
      std::cerr << "警告: 保存图片失败 " << outputPath << "\n";
    }
  }

  std::cout << "========== Demo 结束 ==========\n";
  return 0;
}
