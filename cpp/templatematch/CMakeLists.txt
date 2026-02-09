# 模板匹配模块：输出 libtemplatematch.so / templatematch.dll
cmake_minimum_required(VERSION 3.14)
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
target_link_libraries(templatematch PUBLIC ${OpenCV_LIBS} ocrdetect_common)

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

# 安装
install(TARGETS templatematch
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
install(DIRECTORY include/ DESTINATION include)
