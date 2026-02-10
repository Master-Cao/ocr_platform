/**
 * @file test_ocr.cpp
 * @brief 纯 OCR 测试程序
 *
 * 用法: test_ocr <模型目录> <图片路径>
 * 示例: test_ocr ../OcrDetect/models test.png
 */
#include <ocrdetect/OcrEngine.hpp>
#include <ocrdetect/ConfigLoader.hpp>
#include <opencv2/opencv.hpp>
#include <vector>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <cstdio>
#include <filesystem>

static void drawOcrResult(const cv::Mat& orgImg,
                          const std::vector<ocrdetect::TextBlock>& blocks,
                          const std::string& savePath) {
    namespace fs = std::filesystem;
    const fs::path outDir("/var/ocr/images");

    // 确保输出目录存在（若不存在则尝试创建）
    std::error_code ec;
    if (!fs::exists(outDir)) {
        if (!fs::create_directories(outDir, ec) && ec) {
            std::fprintf(stderr,
                         "failed to create directory %s: %s\n",
                         outDir.string().c_str(), ec.message().c_str());
            std::fprintf(stderr,
                         "hint: please check permissions on /var/ocr and /var/ocr/images\n");
            return;
        }
    }

    cv::Mat visImg = orgImg.clone();
    for (size_t i = 0; i < blocks.size(); i++) {
        const auto& b = blocks[i];
        std::vector<cv::Point> pts;
        for (int k = 0; k < 4; k++)
            pts.push_back(cv::Point(static_cast<int>(b.box[k][0]),
                                    static_cast<int>(b.box[k][1])));
        cv::polylines(visImg, pts, true, cv::Scalar(0, 255, 0), 2);
        char label[32];
        std::snprintf(label, sizeof(label), "[%zu]", i);
        cv::putText(visImg, label, pts[0],
                    cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    cv::Scalar(0, 0, 255), 1, cv::LINE_AA);
    }

    fs::path outPath = outDir / savePath;
    if (cv::imwrite(outPath.string(), visImg))
        std::printf("saved result image: %s\n", outPath.string().c_str());
    else {
        std::fprintf(stderr, "failed to save: %s\n", outPath.string().c_str());
        std::fprintf(stderr,
                     "hint: please check write permission for /var/ocr/images\n");
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("usage: %s <models_dir> <image_path> [--config-dir <dir>]\n", argv[0]);
        printf("example: %s ../OcrDetect/models test.png\n", argv[0]);
        return 1;
    }
    std::string modelsDir = argv[1];
    std::string imagePath = argv[2];
    std::string configDir = "config";
    for (int i = 3; i < argc; i++) {
        if (std::string(argv[i]) == "--config-dir" && i + 1 < argc)
            configDir = argv[++i];
    }
    std::vector<std::string> ocrPaths = { configDir + "/ocrdetect.conf", "config/ocrdetect.conf", "../config/ocrdetect.conf" };
    std::string ocrLoaded;
    auto ocrOpt = ocrdetect::loadOcrDetectConfigFromFileWithFallback(ocrPaths, &ocrLoaded);

    printf("models_dir: %s\n", modelsDir.c_str());
    ocrdetect::OcrEngine engine(modelsDir);
    if (!engine.valid()) {
        fprintf(stderr, "ERROR: failed to init OCR engine, check models_dir: %s\n", modelsDir.c_str());
        return 1;
    }
    printf("OCR engine initialized.\n");
    engine.setNumThreads(ocrOpt.num_threads);

    cv::Mat img = cv::imread(imagePath);
    if (img.empty()) {
        fprintf(stderr, "ERROR: cannot read image: %s\n", imagePath.c_str());
        return 1;
    }
    printf("image: %s (%d x %d)\n", imagePath.c_str(), img.cols, img.rows);

    auto t0 = std::chrono::high_resolution_clock::now();
    auto blocks = engine.detect(img, ocrOpt);
    auto t1 = std::chrono::high_resolution_clock::now();
    // 以毫秒为单位统计耗时
    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    printf("total time: %.2f ms\n", elapsed_ms);
    printf("result: %zu boxes detected\n", blocks.size());
    // 仅输出文字内容和置信度
    for (size_t i = 0; i < blocks.size(); i++) {
        const auto& b = blocks[i];
        printf("text: %s, score: %.4f\n", b.text.c_str(), b.confidence);
    }

    // 输出图像文件名 = 原图名 + "_result.jpg"
    std::filesystem::path inPath(imagePath);
    std::string outName = inPath.stem().string() + "_result.jpg";
    drawOcrResult(img, blocks, outName);
    return 0;
}
