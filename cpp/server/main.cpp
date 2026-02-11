/**
 * C++ 服务端：加载 TM + OCR、预热、HTTP POST /api 按 instruction 分发。
 * 协议见 proto/api.md。
 */
#include <ocrdetect/OcrEngine.hpp>
#include <ocrdetect/ConfigLoader.hpp>
#include <templatematch/Matcher.hpp>
#include <templatematch/ConfigLoader.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include "base64.h"
#include "server_config.h"
#include "httplib.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <chrono>

namespace {

std::string config_dir = "config";
std::string ocr_models_dir;
std::unique_ptr<ocrdetect::OcrEngine> g_ocr;
std::unique_ptr<templatematch::Matcher> g_tm;
ocrdetect::OcrDetectOptions g_ocr_opt;

bool load_engines() {
  std::vector<std::string> tm_paths = {
    config_dir + "/templatematch.conf",
    "config/templatematch.conf",
    "../config/templatematch.conf",
  };
  std::string tm_loaded;
  auto tm_params = templatematch::loadParamsFromFileWithFallback(tm_paths, &tm_loaded);
  std::cout << "[config] templatematch: " << tm_loaded << std::endl;

  if (ocr_models_dir.empty()) {
    std::cerr << "OCR models_dir not set. Set via --models <dir>" << std::endl;
    return false;
  }
  std::vector<std::string> ocr_paths = {
    config_dir + "/ocrdetect.conf",
    "config/ocrdetect.conf",
    "../config/ocrdetect.conf",
  };
  std::string ocr_loaded;
  g_ocr_opt = ocrdetect::loadOcrDetectConfigFromFileWithFallback(ocr_paths, &ocr_loaded);
  std::cout << "[config] ocrdetect: " << ocr_loaded << std::endl;

  g_ocr = std::make_unique<ocrdetect::OcrEngine>(ocr_models_dir);
  if (!g_ocr->valid()) {
    std::cerr << "OcrEngine init failed." << std::endl;
    return false;
  }
  g_ocr->setNumThreads(g_ocr_opt.num_threads);

  g_tm = std::make_unique<templatematch::Matcher>(tm_params);
  if (!g_tm->valid()) {
    std::cerr << "Matcher init failed." << std::endl;
    return false;
  }
  return true;
}

void warmup() {
  cv::Mat dummy(64, 256, CV_8UC3);
  dummy.setTo(cv::Scalar(255, 255, 255));
  std::cout << "[warmup] OCR..." << std::endl;
  (void)g_ocr->detect(dummy, g_ocr_opt, true);
  std::cout << "[warmup] TM..." << std::endl;
  g_tm->setTemplate(dummy);
  (void)g_tm->match(dummy);
  std::cout << "[warmup] done." << std::endl;
}

cv::Mat decode_image(const std::string& b64) {
  auto bin = server::base64_decode(b64);
  if (bin.empty()) return cv::Mat();
  cv::Mat raw(1, static_cast<int>(bin.size()), CV_8UC1, bin.data());
  return cv::imdecode(raw, cv::IMREAD_COLOR);
}

nlohmann::json error_response(int code, const std::string& message, const nlohmann::json& id) {
  return {
    {"jsonrpc", "2.0"},
    {"error", {{"code", code}, {"message", message}}},
    {"id", id}
  };
}

nlohmann::json handle_tm_only(const nlohmann::json& params) {
  std::string scene_b64 = params.value("scene_image", "");
  std::string tmpl_b64 = params.value("template_image", "");
  if (scene_b64.empty() || tmpl_b64.empty())
    throw std::runtime_error("tm_only requires scene_image and template_image");
  cv::Mat scene = decode_image(scene_b64);
  cv::Mat tmpl = decode_image(tmpl_b64);
  if (scene.empty() || tmpl.empty())
    throw std::runtime_error("Invalid image base64");
  if (!g_tm->setTemplate(tmpl))
    throw std::runtime_error("tm set_template failed");
  auto matches = g_tm->match(scene);
  nlohmann::json arr = nlohmann::json::array();
  for (const auto& m : matches) {
    arr.push_back({
      {"left_top", std::vector<double>{m.left_top_x, m.left_top_y}},
      {"right_top", std::vector<double>{m.right_top_x, m.right_top_y}},
      {"right_bottom", std::vector<double>{m.right_bottom_x, m.right_bottom_y}},
      {"left_bottom", std::vector<double>{m.left_bottom_x, m.left_bottom_y}},
      {"center", std::vector<double>{m.center_x, m.center_y}},
      {"angle", m.angle},
      {"score", m.score}
    });
  }
  return {{"instruction", "tm_only"}, {"matches", arr}, {"count", arr.size()}};
}

nlohmann::json handle_ocr_only(const nlohmann::json& params) {
  std::string img_b64 = params.value("image", "");
  if (img_b64.empty()) throw std::runtime_error("ocr_only requires image");
  cv::Mat img = decode_image(img_b64);
  if (img.empty()) throw std::runtime_error("Invalid image base64");
  auto t0 = std::chrono::high_resolution_clock::now();
  auto blocks = g_ocr->detect(img, g_ocr_opt, true);
  auto t1 = std::chrono::high_resolution_clock::now();
  double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  nlohmann::json arr = nlohmann::json::array();
  for (const auto& b : blocks) {
    nlohmann::json box = nlohmann::json::array();
    for (int i = 0; i < 4; i++)
      box.push_back({b.box[i][0], b.box[i][1]});
    arr.push_back({
      {"box", box},
      {"text", b.text},
      {"confidence", b.confidence},
      {"box_score", b.box_score}
    });
  }
  return {
    {"instruction", "ocr_only"},
    {"blocks", arr},
    {"count", arr.size()},
    {"detect_time_ms", ms}
  };
}

nlohmann::json handle_tm_then_ocr(const nlohmann::json& params) {
  std::string scene_b64 = params.value("scene_image", "");
  std::string tmpl_b64 = params.value("template_image", "");
  if (scene_b64.empty() || tmpl_b64.empty())
    throw std::runtime_error("tm_then_ocr requires scene_image and template_image");
  cv::Mat scene = decode_image(scene_b64);
  cv::Mat tmpl = decode_image(tmpl_b64);
  if (scene.empty() || tmpl.empty())
    throw std::runtime_error("Invalid image base64");
  if (!g_tm->setTemplate(tmpl))
    throw std::runtime_error("tm set_template failed");
  auto matches = g_tm->match(scene);
  nlohmann::json regions = nlohmann::json::array();
  for (const auto& m : matches) {
    std::vector<cv::Point2f> pts = {
      cv::Point2f(static_cast<float>(m.left_top_x), static_cast<float>(m.left_top_y)),
      cv::Point2f(static_cast<float>(m.right_top_x), static_cast<float>(m.right_top_y)),
      cv::Point2f(static_cast<float>(m.right_bottom_x), static_cast<float>(m.right_bottom_y)),
      cv::Point2f(static_cast<float>(m.left_bottom_x), static_cast<float>(m.left_bottom_y))
    };
    cv::Rect roi = cv::boundingRect(pts);
    if (roi.x < 0 || roi.y < 0 || roi.x + roi.width > scene.cols || roi.y + roi.height > scene.rows) {
      regions.push_back({
        {"match", {{"left_top", std::vector<double>{m.left_top_x, m.left_top_y}}, {"right_top", std::vector<double>{m.right_top_x, m.right_top_y}},
          {"right_bottom", std::vector<double>{m.right_bottom_x, m.right_bottom_y}}, {"left_bottom", std::vector<double>{m.left_bottom_x, m.left_bottom_y}},
          {"center", std::vector<double>{m.center_x, m.center_y}}, {"angle", m.angle}, {"score", m.score}}},
        {"ocr", {{"blocks", nlohmann::json::array()}, {"count", 0}}}
      });
      continue;
    }
    cv::Mat crop = scene(roi).clone();
    auto blocks = g_ocr->detect(crop, g_ocr_opt, true);
    nlohmann::json ocr_arr = nlohmann::json::array();
    for (const auto& b : blocks) {
      nlohmann::json box = nlohmann::json::array();
      for (int i = 0; i < 4; i++) box.push_back({b.box[i][0], b.box[i][1]});
      ocr_arr.push_back({{"box", box}, {"text", b.text}, {"confidence", b.confidence}, {"box_score", b.box_score}});
    }
    regions.push_back({
      {"match", {{"left_top", std::vector<double>{m.left_top_x, m.left_top_y}}, {"right_top", std::vector<double>{m.right_top_x, m.right_top_y}},
        {"right_bottom", std::vector<double>{m.right_bottom_x, m.right_bottom_y}}, {"left_bottom", std::vector<double>{m.left_bottom_x, m.left_bottom_y}},
        {"center", std::vector<double>{m.center_x, m.center_y}}, {"angle", m.angle}, {"score", m.score}}},
      {"ocr", {{"blocks", ocr_arr}, {"count", ocr_arr.size()}}}
    });
  }
  return {{"instruction", "tm_then_ocr"}, {"regions", regions}, {"match_count", regions.size()}};
}

void handle_api(const httplib::Request& req, httplib::Response& res) {
  res.set_header("Content-Type", "application/json");
  nlohmann::json body;
  try {
    body = nlohmann::json::parse(req.body);
  } catch (const std::exception& e) {
    res.set_content(error_response(-32700, std::string("Parse error: ") + e.what(), nullptr).dump(), "application/json");
    return;
  }
  if (!body.contains("instruction")) {
    res.set_content(error_response(-32600, "Missing instruction", body.value("id", nullptr)).dump(), "application/json");
    return;
  }
  std::string instruction = body["instruction"].get<std::string>();
  nlohmann::json params = body.value("params", nlohmann::json::object());
  auto id = body.value("id", nullptr);
  try {
    nlohmann::json result;
    if (instruction == "tm_only")
      result = handle_tm_only(params);
    else if (instruction == "ocr_only")
      result = handle_ocr_only(params);
    else if (instruction == "tm_then_ocr")
      result = handle_tm_then_ocr(params);
    else {
      res.set_content(error_response(-32600, "Unknown instruction: " + instruction, id).dump(), "application/json");
      return;
    }
    nlohmann::json out = {{"jsonrpc", "2.0"}, {"result", result}, {"id", id}};
    res.set_content(out.dump(), "application/json");
  } catch (const std::exception& e) {
    res.set_content(error_response(-32602, e.what(), id).dump(), "application/json");
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--config-dir" && i + 1 < argc) config_dir = argv[++i];
    else if (arg == "--models" && i + 1 < argc) ocr_models_dir = argv[++i];
  }
  if (ocr_models_dir.empty()) {
    std::cerr << "Usage: " << (argv[0] ? argv[0] : "ocr_server")
              << " --models <ocr_models_dir> [--config-dir <dir>]\n";
    return 1;
  }
  if (!load_engines()) return 1;
  warmup();

  // 服务端配置：server/config/server.conf（与 Python 端结构一致）
  server::ConfigMap server_cfg;
  std::vector<std::string> server_conf_paths = {
    config_dir + "/server.conf",
    config_dir + "/../server/config/server.conf",
    "config/server.conf",
    "server/config/server.conf",
  };
  if (server::load_server_config(server_conf_paths, server_cfg))
    std::cout << "[config] server.conf loaded" << std::endl;
  std::string http_host = server::config_get(server_cfg, "server", "host", "0.0.0.0");
  int http_port = server::config_get_int(server_cfg, "server", "port", 8080);

  // 传输层：HTTP 已实现；MQTT / ZeroMQ 预留位置见 server/transport/README.md
  // 可在此根据 server_cfg [transport] mqtt_enabled/zeromq_enabled 启动对应线程

  httplib::Server svr;
  svr.Post("/api", handle_api);
  svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
    res.set_content("{\"status\":\"ok\"}", "application/json");
  });
  std::cout << "HTTP server " << http_host << ":" << http_port << " (POST /api, GET /health)" << std::endl;
  svr.listen(http_host.c_str(), http_port);
  return 0;
}
