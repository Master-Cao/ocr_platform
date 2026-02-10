cmake_minimum_required(VERSION 3.14)

# test_ocr: 纯 OCR 测试，仅依赖 ocrdetect
if(TARGET ocrdetect)
  add_executable(test_ocr test_ocr.cpp)
  target_link_libraries(test_ocr PRIVATE ocrdetect)
endif()

# demo / demo_tm_ocr: 模板匹配 + OCR，需 templatematch 和 ocrdetect
if(TARGET templatematch AND TARGET ocrdetect)
  set(EXAMPLES_RPATH "\$ORIGIN/../lib")
  add_executable(demo demo.cpp)
  target_link_libraries(demo PRIVATE templatematch ocrdetect)
  set_target_properties(demo PROPERTIES BUILD_RPATH ${EXAMPLES_RPATH} INSTALL_RPATH ${EXAMPLES_RPATH} BUILD_WITH_INSTALL_RPATH TRUE)

  add_executable(demo_tm_ocr demo_tm_ocr.cpp)
  target_link_libraries(demo_tm_ocr PRIVATE templatematch ocrdetect)
  set_target_properties(demo_tm_ocr PROPERTIES BUILD_RPATH ${EXAMPLES_RPATH} INSTALL_RPATH ${EXAMPLES_RPATH} BUILD_WITH_INSTALL_RPATH TRUE)
endif()
