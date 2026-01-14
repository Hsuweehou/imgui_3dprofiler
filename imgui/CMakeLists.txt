cmake_minimum_required(VERSION 3.22)

project(gui)

set(CMAKE_CXX_STANDARD 17)

# ############################################################
# # Should be used in the TOP LEVEL of CMakeLists.txt file.
# # Set the install root path as the cmake binary dir
# ############################################################
if( CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT )
  message(STATUS "Setting default CMAKE_INSTALL_PREFIX path to ${CMAKE_BINARY_DIR}/install")
  set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE STRING "The path to use for make install" FORCE)
endif()

# ##########################################################
# # Setting path for 3rd party libraries
# ##########################################################
set(THIRD_PARTY_LIBRARY_DIR ${CMAKE_SOURCE_DIR}/../../../3rdParty CACHE PATH "The path to third party library")  # set to the relative path to current CmakeLists.txt
if (NOT DEFINED CMAKE_BUILD_TYPE)
    message(STATUS "CMAKE_BUILD_TYPE is not defined! Set it as Release by default.")
    set(CMAKE_BUILD_TYPE "Release")
endif()


include_directories(
    ${CMAKE_SOURCE_DIR}/gui
    ${THIRD_PARTY_LIBRARY_DIR}/glfw-3.4/include
    ${THIRD_PARTY_LIBRARY_DIR}/imgui_gl_1.92
    ${THIRD_PARTY_LIBRARY_DIR}/imgui_gl_1.92/backends
    ${THIRD_PARTY_LIBRARY_DIR}/imgui_gl_1.92/extension
    # ${THIRD_PARTY_LIBRARY_DIR}/imgui_gl_1.92/extension/imguifile
    # ${THIRD_PARTY_LIBRARY_DIR}/imgui_gl_1.92/extension/imguifile
    ${THIRD_PARTY_LIBRARY_DIR}/glm
)

message(STATUS "CMAKE_SOURCE_DIR:"${CMAKE_SOURCE_DIR})
link_directories(
    ${THIRD_PARTY_LIBRARY_DIR}/glfw-3.4/lib-vc2019
    ${THIRD_PARTY_LIBRARY_DIR}/glm/lib
)

# add_library(${PROJECT_NAME} 
#     ${CMAKE_SOURCE_DIR}/gui/src/Application.cpp
#     # Add header files is for IDE
#     ${CMAKE_SOURCE_DIR}/gui/include/Application.h
#     ${CMAKE_SOURCE_DIR}/gui/include/IconsFontAwesome6.h

# )

file(GLOB_RECURSE IMGUI_SRCS 
    ${THIRD_PARTY_LIBRARY_DIR}/imgui_gl_1.92/*.cpp)

file(GLOB APP_SRCS 
    ${CMAKE_SOURCE_DIR}/gui/src/*.cpp)

file(GLOB MAIN_SRCS 
    ${CMAKE_SOURCE_DIR}/gui/src)

file(GLOB SCANNER_SRCS 
    ${CMAKE_SOURCE_DIR}/scanner_l/src)

# add_library(${PROJECT_NAME} 
#     ${CMAKE_SOURCE_DIR}/gui/include/Application.h
#     # ${CMAKE_SOURCE_DIR}/gui/include/Application.h
# )

# include_directories(${CMAKE_SOURCE_DIR}/gui/include

# )

message(STATUS "APP_SRCS:"${APP_SRCS})
message(STATUS "MAIN_SRCS:"${MAIN_SRCS})
message(STATUS "SCANNER_SRCS:"${SCANNER_SRCS})
message(STATUS "IMGUI_SRCS:"${IMGUI_SRCS})
# add_executable(${PROJECT_NAME} WIN32 src/gui/main.cpp ${IMGUI_SRCS}) #添加WIN32参数就不会显示控制台界面了
set(TEST_MAIN Scanner_Server)
add_executable(${PROJECT_NAME}
    ${MAIN_SRCS}/main.cpp
    ${IMGUI_SRCS}
    ${APP_SRCS}
    # ${MAIN_SRCS}/font_msyh.c
    ${SCANNER_SRCS}/${TEST_MAIN}.cpp
    ${CMAKE_SOURCE_DIR}/scanner_l/include/scanner_l/Scanner_Server.h
    # ${MAIN_SRCS}/single_scanner.cpp
    # ${SCANNER_SRCS}/Brassiere_Cup_Scanner.cpp
    )


target_include_directories(${PROJECT_NAME} PUBLIC 
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    ${CMAKE_SOURCE_DIR}/scanner_l/include/scanner_l
    ${CMAKE_SOURCE_DIR}/gui/include
)
    
target_link_libraries(${PROJECT_NAME}
        glfw3
        opengl32
        glog::glog
        ScannerCtrl::scanner_l
        serial::serial
        MitsubishiPLCLink
        Eigen3::Eigen
        libmodbus
        # Boost::boost
)  # 链接库文件

#注释掉可以保留运行时的终端
# Set_Target_Properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")