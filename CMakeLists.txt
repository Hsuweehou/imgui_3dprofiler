cmake_minimum_required(VERSION 3.18)

project(SingleScannerGui)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_DEBUG_POSTFIX d)


# using gRPC installed by vcpkg, but this setting is no success. Setting this variable CMAKE_TOOLCHAIN_FILE
# through GUI CMAKE work.
# set(VCPKG_ROOT "C:/Users/Depthvision_user/Work/Code/Reconstruct/3rdParty/grpc_vcpkg")
# set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE )

# Make glog recognized as dll
add_definitions(-DGLOG_USE_GLOG_EXPORT)

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
set(THIRD_PARTY_LIBRARY_DIR ${CMAKE_SOURCE_DIR}/../../3rdParty CACHE PATH "The path of third party libraries.")  # set to the relative path to current CmakeLists.txt
if (NOT DEFINED CMAKE_BUILD_TYPE)
    message("CMAKE_BUILD_TYPE is not defined! Set it as Debug by default.")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "The build type (Important for gRPC) for project.")
    # message("CMAKE_BUILD_TYPE is not defined! Set it as Release by default.")
    # set(CMAKE_BUILD_TYPE "Release" CACHE STRING "The build type (Important for gRPC) for project.")
endif()

# JSON
set(nlohmann_json_INCLUDE_DIRS ${THIRD_PARTY_LIBRARY_DIR}/nlohmann/include CACHE PATH "The path of nlohmann_json")
include_directories( ${nlohmann_json_INCLUDE_DIRS} )

# #OpenCV
set(OpenCV_DIR ${THIRD_PARTY_LIBRARY_DIR}/OpenCV/opencv4/build/x64/vc15/lib)
message(STATUS "OpenCV_DIR = ${OpenCV_DIR}")
find_package(OpenCV 4.5.1 REQUIRED)
if(OpenCV_FOUND)
    message ("opencv found")
else()
    message (FATAL_ERROR "Cannot find opencv")
endif()

# #gflags
set(gflags_DIR ${THIRD_PARTY_LIBRARY_DIR}/gflags/lib/cmake/gflags)
#message(STATUS "gflags_DIR = ${gflags_DIR}")
find_package(gflags REQUIRED)
if(gflags_FOUND)
    message ("gflags found")
else()
    message (FATAL_ERROR "Cannot find gflags")
endif()

# #glog
set(glog_DIR ${THIRD_PARTY_LIBRARY_DIR}/glog/lib/cmake/glog)
#message(STATUS "glog_DIR = ${glog_DIR}")
find_package(glog REQUIRED)
if(glog_FOUND)
    message ("glog found")
else()
    message (FATAL_ERROR "Cannot find glog")
endif()

# apriltag
set(apriltag_DIR ${THIRD_PARTY_LIBRARY_DIR}/apriltag/share/apriltag/cmake)
message(STATUS "apriltag_DIR = ${apriltag_DIR}")
find_package(apriltag  REQUIRED)
# target_link_libraries( ${PROJECT_NAME} PRIVATE apriltag::apriltag)

#libtiff
set(TIFF_DIR ${THIRD_PARTY_LIBRARY_DIR}/tiff/lib/cmake/tiff)
message(STATUS "TIFF_DIR = " ${TIFF_DIR})
find_package(TIFF REQUIRED PATHS ${TIFF_DIR})

if(TIFF_FOUND)
    message("TIFF ${TIFF_VERSION} found.")
else()
    message(FATAL_ERROR "TIFF not found.")
endif(TIFF_FOUND)

# Eigen3
set(Eigen3_DIR ${THIRD_PARTY_LIBRARY_DIR}/Eigen/share/eigen3/cmake)
message(STATUS "Eigen3_DIR = "${Eigen3_DIR})
find_package(Eigen3 REQUIRED PATHS ${Eigen3_DIR})
if(Eigen3_FOUND)
	message("eigen3 found!")
else()
	message(FATAL_ERROR "Can not find eigen3!")
endif()

# Ceres
# find_package(Ceres REQUIRED)
set(Ceres_DIR ${THIRD_PARTY_LIBRARY_DIR}/Ceres/lib/cmake/Ceres)
message(STATUS "Ceres_DIR = " ${Ceres_DIR})
find_package(Ceres REQUIRED PATHS ${Ceres_DIR})
if(Ceres_FOUND)
	message("ceres found!")
else()
	message(FATAL_ERROR "Can not find ceres")
endif()

#PCL
set(PCL_DIR ${THIRD_PARTY_LIBRARY_DIR}/PCL/cmake)
find_package(PCL REQUIRED PATHS ${PCL_DIR})
include_directories(${PCL_INCLUDE_DIRS})
link_directories(${PCL_LIBRARY_DIRS})
add_definitions(${PCL_DEFINITIONS})
#target_link_libraries( ${PROJECT_NAME} PRIVATE ${PCL_LIBRARIES})

set(CCCoreLib_DIR ${THIRD_PARTY_LIBRARY_DIR}/CCCoreLib/lib/cmake/CCCoreLib)
message(STATUS "CCCoreLib_DIR = " ${CCCoreLib_DIR})
find_package(CCCoreLib REQUIRED PATHS ${CCCoreLib_DIR})
if(CCCoreLib_FOUND)
	message("CCCoreLib found!")
else()
	message(FATAL_ERROR "Can not find CCCoreLib")
endif()
# include_directories(${CCCoreLib_INCLUDE_DIRS})
# target_link_libraries( ${PROJECT_NAME} PRIVATE ${CCCoreLib_INCLUDE_DIRS})

# 配置 serial 库（使用 Config 模式）
set(serial_DIR ${CMAKE_CURRENT_SOURCE_DIR}/plc_serial/serial/lib/cmake)
message(STATUS "serial_DIR: ${serial_DIR}")
find_package(serial REQUIRED CONFIG)
if(NOT serial_FOUND)
    message(FATAL_ERROR "未找到 serial 库，请确认 serial 库已正确安装")
endif()

# 创建静态库目标 MitsubishiPLCLink
add_library(MitsubishiPLCLink STATIC
    plc_serial/src/mitsubishi_plc_fx_link.cpp
    plc_serial/include/mitsubishi_plc_fx_link.h
)
# 针对 Debug 模式，在库文件名称后添加 "d" 后缀
set_target_properties(MitsubishiPLCLink PROPERTIES DEBUG_POSTFIX "d")

############################################################
# Link libraries to be tested
############################################################

add_definitions(-DWIN32_LEAN_AND_MEAN)

# modbus lib
message("modbus DLL IMPORT: "${THIRD_PARTY_LIBRARY_DIR}/modbus/bin/Release/modbus.dll)
add_library(libmodbus SHARED IMPORTED GLOBAL)
if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Release"))
    set_target_properties(libmodbus PROPERTIES
        IMPORTED_LOCATION ${THIRD_PARTY_LIBRARY_DIR}/modbus/bin/Release/modbus.dll
        IMPORTED_IMPLIB ${THIRD_PARTY_LIBRARY_DIR}/modbus/bin/Release/modbus.lib
    )
elseif(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
    set_target_properties(libmodbus PROPERTIES
    IMPORTED_LOCATION ${THIRD_PARTY_LIBRARY_DIR}/modbus/bin/Debug/modbus.dll
    IMPORTED_IMPLIB ${THIRD_PARTY_LIBRARY_DIR}/modbus/bin/Debug/modbus.lib
    )
endif()

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

# 安装目标及导出设置
install(
    TARGETS MitsubishiPLCLink
    EXPORT MitsubishiPLCLinkTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# 导出目标及生成 Config 文件
install(
    EXPORT MitsubishiPLCLinkTargets
    NAMESPACE MitsubishiPLCLink::
    FILE MitsubishiPLCLinkConfig.cmake
    DESTINATION lib/cmake
)

# set(GTest_DIR ${THIRD_PARTY_LIBRARY_DIR}/gtest/lib/cmake/GTest)
# message("GTest dir:"${GTest_DIR})
# find_package(GTest REQUIRED)

# if(GTest_FOUND)
#     message("GTest ${GTest_VERSION} found")
# else()
#     message(FATAL_ERROR "Cannot find GTest")
# endif()


# boost
# set(Boost_DIR ${THIRD_PARTY_LIBRARY_DIR}/Ceres/lib/cmake/Ceres)
# message(STATUS "Boost_DIR = " ${Boost_DIR})
# find_package(Boost_DIR REQUIRED PATHS ${Boost_DIR})
# if(Boost_FOUND)
# 	message("Boost found!")
# else()
# 	message(FATAL_ERROR "Can not find Boost")
# endif()

# # Set postfix for debug file.
set(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "Set debug library postfix")

# ---------- Setup output Directories -------------------------
message(STATUS ${${PROJECT_NAME}_BINARY_DIR})
if(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
    ${${PROJECT_NAME}_BINARY_DIR}/Bin
    CACHE PATH
    "Single Directory for all Libraries"
    )
endif()

# --------- Setup the Executable output Directory -------------
if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
    ${${PROJECT_NAME}_BINARY_DIR}/Bin
    CACHE PATH
    "Single Directory for all Executables."
    )
endif()

# --------- Setup the Executable output Directory -------------
if(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
    ${${PROJECT_NAME}_BINARY_DIR}/Lib
    CACHE PATH
    "Single Directory for all static libraries."
    )
endif()

############################################################
# Add GUI unit 
############################################################
add_subdirectory(imgui)

############################################################
# Add original scanner_l
############################################################
add_subdirectory(scanner_l)

############################################################
# Add test unit 
############################################################
add_subdirectory(test)

############################################################
# Add plc_test unit 
############################################################
# add_subdirectory(pcl_serial)
