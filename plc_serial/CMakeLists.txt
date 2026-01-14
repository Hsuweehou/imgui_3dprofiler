cmake_minimum_required(VERSION 3.10)

project(pcl_serial)


message(STATUS "Setting default CMAKE_INSTALL_PREFIX path to ${CMAKE_BINARY_DIR}/install")
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE STRING "The path to use for make install" FORCE)


set(POSITION_INDEPENDENT_CODE True)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# 设置源文件和执行字符集为 UTF-8 (MSVC)
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

if(MSVC)
    add_definitions("-DUNICODE" "-D_UNICODE")
endif()

project(MitsubishiPLCLink VERSION 1.0 LANGUAGES CXX)

# 使用 C++11 或更新的标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 配置 serial 库（使用 Config 模式）
set(serial_DIR ${CMAKE_CURRENT_SOURCE_DIR}/serial/lib/cmake)
message(STATUS "serial_DIR: ${serial_DIR}")
find_package(serial REQUIRED CONFIG)
if(NOT serial_FOUND)
    message(FATAL_ERROR "未找到 serial 库，请确认 serial 库已正确安装")
endif()

# 创建静态库目标 MitsubishiPLCLink
add_library(MitsubishiPLCLink STATIC
    src/mitsubishi_plc_fx_link.cpp
    include/mitsubishi_plc_fx_link.h
)
# 针对 Debug 模式，在库文件名称后添加 "d" 后缀
set_target_properties(MitsubishiPLCLink PROPERTIES DEBUG_POSTFIX "d")

# 使用生成器表达式指定 include 目录：
#  - BUILD_INTERFACE：编译时引用源目录中的头文件
#  - INSTALL_INTERFACE：安装后使用的相对路径（需确保 install() 时将头文件复制到该位置）
target_include_directories(MitsubishiPLCLink
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/serial/include/serial>
        $<INSTALL_INTERFACE:serial/include/serial>
)

# 链接 serial 库（serial 库导出时已经包含了自身的 include 目录，但如果你项目中需要 serial 的额外头文件，则可这样写）
target_link_libraries(MitsubishiPLCLink PRIVATE serial::serial)

# 添加子目录（例如测试）
add_subdirectory(test)

#
# 安装规则
#

# 安装目标及导出设置
install(
    TARGETS MitsubishiPLCLink
    EXPORT MitsubishiPLCLinkTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# 安装项目头文件（确保安装路径与 INSTALL_INTERFACE 中使用的路径一致）
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
)

# 如果项目中也需要安装 serial 部分的头文件，请确保 serial 库安装时也处理此事，
# 否则可不安装下面这行，如果 serial 库由第三方管理，该行可以删除
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/serial/include/serial
    DESTINATION serial/include
)

# 导出目标及生成 Config 文件
install(
    EXPORT MitsubishiPLCLinkTargets
    NAMESPACE MitsubishiPLCLink::
    FILE MitsubishiPLCLinkConfig.cmake
    DESTINATION lib/cmake
)
