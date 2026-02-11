# C++ 服务端：加载 TM + OCR、预热、HTTP /api
cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)

if(NOT TARGET ocrdetect OR NOT TARGET templatematch)
  message(FATAL_ERROR "server requires ocrdetect and templatematch; build from cpp root with add_subdirectory(server)")
endif()

# nlohmann/json (header-only)
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.2
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# cpp-httplib (single header in include/)
FetchContent_Declare(
  cpp_httplib
  URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.14.3.zip
)
FetchContent_Populate(cpp_httplib)

set(SERVER_SOURCES main.cpp base64.cpp)
add_executable(ocr_server ${SERVER_SOURCES})

target_include_directories(ocr_server PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${cpp_httplib_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../OcrDetect/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../templatematch/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_link_libraries(ocr_server PRIVATE
  ocrdetect
  templatematch
  nlohmann_json::nlohmann_json
)

set(SERVER_RPATH "\$ORIGIN/../lib")
set_target_properties(ocr_server PROPERTIES
  BUILD_RPATH ${SERVER_RPATH}
  INSTALL_RPATH ${SERVER_RPATH}
  BUILD_WITH_INSTALL_RPATH TRUE
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
