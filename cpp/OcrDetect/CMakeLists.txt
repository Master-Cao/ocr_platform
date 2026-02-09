# OCR 检测模块：输出 libocrdetect.so / ocrdetect.dll
cmake_minimum_required(VERSION 3.14)

# OpenCV
find_package(OpenCV REQUIRED)

# ONNX Runtime
# build.sh (Linux) 会在构建前根据架构自动解压对应的 tgz
# 用户也可通过 -DOnnxRuntime_DIR=<路径> 手动指定（需含 include/ 和 lib/）
if(NOT DEFINED OnnxRuntime_DIR)
  # 根据当前平台和架构确定匹配模式
  if(WIN32)
    set(_ort_os "win")
  else()
    set(_ort_os "linux")
  endif()

  # CMAKE_SYSTEM_PROCESSOR: x86_64 / AMD64 / aarch64 / arm64 等
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _cpu)
  if(_cpu MATCHES "x86_64|amd64|x64")
    set(_ort_arch "x64")
  elseif(_cpu MATCHES "aarch64|arm64")
    set(_ort_arch "aarch64")
  else()
    set(_ort_arch "${_cpu}")
  endif()

  # 精确匹配当前平台+架构的解压目录
  file(GLOB _ort_dirs "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-${_ort_os}-${_ort_arch}-*")
  # 过滤掉 .tgz / .zip 压缩包，只保留目录
  set(_ort_dirs_filtered "")
  foreach(_d ${_ort_dirs})
    if(IS_DIRECTORY "${_d}")
      list(APPEND _ort_dirs_filtered "${_d}")
    endif()
  endforeach()

  list(LENGTH _ort_dirs_filtered _ort_count)
  if(_ort_count GREATER 0)
    # 取最后一个（版本号最大）
    list(GET _ort_dirs_filtered -1 OnnxRuntime_DIR)
  endif()

  message(STATUS "ONNX Runtime 自动探测: os=${_ort_os} arch=${_ort_arch} -> ${OnnxRuntime_DIR}")
endif()

if(NOT OnnxRuntime_DIR OR NOT EXISTS "${OnnxRuntime_DIR}/include")
  message(FATAL_ERROR
    "ONNX Runtime 未找到！\n"
    "  Linux:   请先运行 ./build.sh 自动解压\n"
    "  Windows: 请手动解压 onnxruntime-win-x64-*.zip 到 OcrDetect/ 目录\n"
    "  或手动指定: cmake -DOnnxRuntime_DIR=<onnxruntime解压目录>"
  )
endif()

set(OnnxRuntime_INCLUDE_DIRS "${OnnxRuntime_DIR}/include")
file(GLOB OnnxRuntime_LIBS
  "${OnnxRuntime_DIR}/lib/libonnxruntime.so*"
  "${OnnxRuntime_DIR}/lib/onnxruntime.lib"
  "${OnnxRuntime_DIR}/lib/onnxruntime.dll"
)
if(NOT OnnxRuntime_LIBS)
  message(FATAL_ERROR "ONNX Runtime 库文件未找到: ${OnnxRuntime_DIR}/lib/")
endif()
message(STATUS "ONNX Runtime: ${OnnxRuntime_DIR}")

set(OCR_SOURCES
  src/ocr_api.cpp
  src/AngleNet.cpp
  src/CrnnNet.cpp
  src/DbNet.cpp
  src/OcrLite.cpp
  src/OcrUtils.cpp
  src/clipper.cpp
)

add_library(ocrdetect SHARED ${OCR_SOURCES})

target_include_directories(ocrdetect
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OnnxRuntime_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(ocrdetect PRIVATE OCRDETECT_OCR_EXPORTS)
target_link_libraries(ocrdetect PUBLIC ${OpenCV_LIBS} ${OnnxRuntime_LIBS} ocrdetect_common)

if(UNIX)
  find_package(OpenMP QUIET)
  if(OpenMP_CXX_FOUND)
    target_link_libraries(ocrdetect PUBLIC OpenMP::OpenMP_CXX)
  endif()
  target_link_libraries(ocrdetect PRIVATE dl)
endif()

set_target_properties(ocrdetect PROPERTIES
  OUTPUT_NAME ocrdetect
  VERSION ${PROJECT_VERSION}
  SOVERSION 1
)

# 运行时能找到 onnx 动态库（若为 so/dll）
if(UNIX)
  set_target_properties(ocrdetect PROPERTIES
    BUILD_RPATH "${OnnxRuntime_DIR}/lib"
    INSTALL_RPATH "${OnnxRuntime_DIR}/lib"
  )
endif()

# 安装主库（移除了 test_rec 可执行程序）
install(TARGETS ocrdetect
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
install(DIRECTORY include/ DESTINATION include)
