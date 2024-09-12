cmake_minimum_required(VERSION 3.16)

# 设置项目名称和使用的语言
project(cython_decode LANGUAGES CXX)

#find_package(Torch REQUIRED)
# 设置C++标准
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-mavx512f -mavx512bw -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

# 设置编译器标志

# 设置构建目录
#set(BUILD_DIR "${CMAKE_BINARY_DIR}/build")
#set(DEBUG_DIR "${CMAKE_BINARY_DIR}/build_debug")

# 文件GLOB收集所有cpp文件，然后移除不需要编译的文件
file(GLOB SOURCES "*.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/mem_extract_c.cpp")
# list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/avx512test.cpp")
# list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_torch.cpp")

# 分别设置Release和Debug目标

add_executable(app ${SOURCES})
#set_target_properties(app PROPERTIES RUNTIME_OUTPUT_DIRECTORY)
target_link_libraries(app pcap)
set_property(TARGET app PROPERTY CXX_STANDARD 17)

add_executable(app_debug ${SOURCES})
#set_target_properties(app_debug PROPERTIES RUNTIME_OUTPUT_DIRECTORY )
target_compile_definitions(app_debug PRIVATE DEBUG=1)
target_link_libraries(app_debug pcap)

#add_executable(test_torch test_torch.cpp)
#arget_link_libraries(test_torch "${TORCH_LIBRARIES}")
#set_property(TARGET test_torch PROPERTY CXX_STANDARD 17)

