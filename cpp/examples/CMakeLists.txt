cmake_minimum_required(VERSION 3.14)

# test_ocr: 纯 OCR 测试，仅依赖 ocrdetect
if(TARGET ocrdetect)
  add_executable(test_ocr test_ocr.cpp)
  target_link_libraries(test_ocr PRIVATE ocrdetect)
endif()

# demo: 模板匹配 + OCR 联合演示，需要 templatematch 和 ocrdetect
if(TARGET templatematch AND TARGET ocrdetect)
  add_executable(demo demo.cpp)
  target_link_libraries(demo PRIVATE templatematch ocrdetect)
endif()
