# 模板匹配模块：输出 libtemplatematch.so / templatematch.dll
# 支持独立构建（仅 templatematch 目录）或作为子项目构建
cmake_minimum_required(VERSION 3.14)
project(templatematch VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(OpenCV REQUIRED)

set(TM_SOURCES
  src/tm_api.cpp
  src/matcher.cpp
  src/base_matcher/base_matcher.cpp
  src/Pattern_Matching/PatternMatching.cpp
)

add_library(templatematch SHARED ${TM_SOURCES})

target_include_directories(templatematch
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(templatematch PRIVATE TEMPLATEMATCH_EXPORTS)
target_link_libraries(templatematch PUBLIC ${OpenCV_LIBS})
if(TARGET ocrdetect_common)
  target_link_libraries(templatematch PUBLIC ocrdetect_common)
endif()

if(UNIX)
  find_package(OpenMP QUIET)
  if(OpenMP_CXX_FOUND)
    target_link_libraries(templatematch PUBLIC OpenMP::OpenMP_CXX)
  endif()
endif()

set_target_properties(templatematch PROPERTIES
  OUTPUT_NAME templatematch
  VERSION ${PROJECT_VERSION}
  SOVERSION 1
)

# demo_tm: 模板匹配测试
add_executable(demo_tm demo/demo_tm.cpp)
target_link_libraries(demo_tm PRIVATE templatematch)
# demo_tm 与 libtemplatematch.so 同目录（仅独立构建时），RPATH \$ORIGIN 即可找到
set_target_properties(demo_tm PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  BUILD_RPATH "\$ORIGIN:\$ORIGIN/../lib"
  INSTALL_RPATH "\$ORIGIN:\$ORIGIN/../lib"
  BUILD_WITH_INSTALL_RPATH TRUE
)
# 独立构建：.so 输出到 bin，与 demo_tm 同目录；作为子项目：.so 输出到 lib，与 ocrdetect 一起
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set_target_properties(templatematch PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

# 安装配置文件到 config/
install(FILES config/templatematch.conf DESTINATION config)
# 安装
install(TARGETS templatematch
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
install(DIRECTORY include/ DESTINATION include)
