# QT_OCR_Service C++: 模板匹配 + OCR 检测
cmake_minimum_required(VERSION 3.14)
project(OcrDetect VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(BUILD_TEMPLATEMATCH "Build templatematch library (template matching)" ON)
option(BUILD_OCRDETECT "Build ocrdetect library (OCR detection)" ON)

if(NOT BUILD_TEMPLATEMATCH AND NOT BUILD_OCRDETECT)
  message(FATAL_ERROR "At least one of BUILD_TEMPLATEMATCH or BUILD_OCRDETECT must be ON")
endif()

add_library(ocrdetect_common INTERFACE)
target_include_directories(ocrdetect_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/common)

if(BUILD_TEMPLATEMATCH)
  add_subdirectory(templatematch)
endif()
if(BUILD_OCRDETECT)
  add_subdirectory(OcrDetect)
endif()

# 复制配置文件到 build/config/，便于运行时从 build/bin 使用 --config-dir ../config
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/config)
if(BUILD_TEMPLATEMATCH)
  configure_file(${CMAKE_SOURCE_DIR}/templatematch/config/templatematch.conf
    ${CMAKE_BINARY_DIR}/config/templatematch.conf COPYONLY)
endif()
if(BUILD_OCRDETECT)
  configure_file(${CMAKE_SOURCE_DIR}/OcrDetect/config/ocrdetect.conf
    ${CMAKE_BINARY_DIR}/config/ocrdetect.conf COPYONLY)
endif()

# examples: test_ocr (OCR only), demo (templatematch + OCR)
add_subdirectory(examples)
