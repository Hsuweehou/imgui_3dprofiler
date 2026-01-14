cmake_minimum_required(VERSION 3.18)

project(scanner_l)

add_definitions(-DWIN32_LEAN_AND_MEAN)


# ##########################################################
# # Setting path for 3rd party libraries
# ##########################################################
if (NOT DEFINED THIRD_PARTY_LIBRARY_DIR)
    # Check if the environment variable is set
    if (DEFINED ENV{THIRD_PARTY_LIBRARY_DIR})
        set(THIRD_PARTY_LIBRARY_DIR $ENV{THIRD_PARTY_LIBRARY_DIR} CACHE PATH "Path to 3rd party libraries")
    else()
        set(THIRD_PARTY_LIBRARY_DIR ${CMAKE_SOURCE_DIR}/../../../3rdParty CACHE PATH "Path to 3rd party libraries")
    endif()
else ()
    set(THIRD_PARTY_LIBRARY_DIR ${THIRD_PARTY_LIBRARY_DIR} CACHE PATH "Path to 3rd party libraries")
endif()
message(STATUS "THIRD_PARTY_LIBRARY_DIR = ${THIRD_PARTY_LIBRARY_DIR}")

# Link to AIRlink SDK
# set(SDK_DIR ${CMAKE_SOURCE_DIR}/sdk_c++_20250219)
set(SDK_DIR ${CMAKE_SOURCE_DIR}/SDK_C++)#direct connect test
#message(STATUS "AIRlink DLL IMPORT: "${SDK_DIR}/bin/AIRLink_Cppd.dll)
add_library(AIRLink_Cpp SHARED IMPORTED GLOBAL)
add_library(CommunicateModule SHARED IMPORTED GLOBAL)
add_library(Scanner_sdk SHARED IMPORTED GLOBAL)
if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Release"))
    set_target_properties(AIRLink_Cpp PROPERTIES
        IMPORTED_LOCATION ${SDK_DIR}/bin/AIRLink_Cpp.dll
        IMPORTED_IMPLIB ${SDK_DIR}/lib/AIRLink_Cpp.lib
    )
    set_target_properties(CommunicateModule PROPERTIES
        IMPORTED_LOCATION ${SDK_DIR}/bin/communication.dll
        IMPORTED_IMPLIB ${SDK_DIR}/lib/communication.lib
    )
    set_target_properties(Scanner_sdk PROPERTIES
        IMPORTED_LOCATION ${SDK_DIR}/bin/lib/scanner_sdk.dll
        IMPORTED_IMPLIB ${SDK_DIR}/lib/scanner_sdk.lib
    )
elseif(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
    set_target_properties(AIRLink_Cpp PROPERTIES
        IMPORTED_LOCATION ${SDK_DIR}/bin/AIRLink_Cppd.dll
        IMPORTED_IMPLIB ${SDK_DIR}/lib/AIRLink_Cppd.lib
    )
    set_target_properties(CommunicateModule PROPERTIES
        IMPORTED_LOCATION ${SDK_DIR}/bin/communicationd.dll
        IMPORTED_IMPLIB ${SDK_DIR}/lib/communicationd.lib
    )
    set_target_properties(Scanner_sdk PROPERTIES
        IMPORTED_LOCATION ${SDK_DIR}/bin/lib/scanner_sdkd.dll
        IMPORTED_IMPLIB ${SDK_DIR}/lib/scanner_sdkd.lib
    )
endif()

message(STATUS "OpenCV_INCLUDE_DIRS: "${OpenCV_INCLUDE_DIRS})
message(STATUS "OpenCV_LIBS: "${OpenCV_LIBS})
message(STATUS "PROJECT_NAME: "${PROJECT_NAME})
message(STATUS "CMAKE_SOURCE_DIR: "${CMAKE_SOURCE_DIR})
###########################################################
# Add EXE test target
############################################################
add_library(${PROJECT_NAME} 
    src/scanner_l_api.cpp
    src/motion_conf.cpp
    src/FileWatcher.cpp
    # src/Scanner_Server.cpp
    # Add header files is for IDE
    include/${PROJECT_NAME}/scanner_l_api.h
    include/${PROJECT_NAME}/type.h
    include/${PROJECT_NAME}/scanner_all_data.h
    include/${PROJECT_NAME}/happly.h
    include/${PROJECT_NAME}/scan_io.h
    include/${PROJECT_NAME}/scan_share_memory.h
    include/${PROJECT_NAME}/motion_conf.h
    include/${PROJECT_NAME}/FileWatcher.h
    ../../plc_serial/include/mitsubishi_plc_fx_link.h
    # include/${PROJECT_NAME}/Scanner_Server.h
)

add_library(ScannerCtrl::scanner_l ALIAS ${PROJECT_NAME})

# Add include dir for current target COMMON
target_include_directories(${PROJECT_NAME} PUBLIC 
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    ${OpenCV_INCLUDE_DIRS}
)


############################################################
# Link libraries to be tested
############################################################
target_link_libraries( ${PROJECT_NAME}
    PUBLIC
        # communication::communication
        # scanner_sdk::scanner_sdk
        AIRLink_Cpp
        CommunicateModule
        Scanner_sdk
        serial::serial
        MitsubishiPLCLink
)

# Add include dir of OpenCV, GLOG to target
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${OpenCV_INCLUDE_DIRS}
        ${GLOG_INCLUDE_PATH}
        ${SDK_DIR}/include
        # ${CMAKE_SOURCE_DIR}/scanner_l/include/scanner_l
        # ${THIRD_PARTY_LIBRARY_DIR}/modbus/include
)

# Add library dir of OpenCV, GLOG to target
target_link_libraries( ${PROJECT_NAME}
    PRIVATE
        ${OpenCV_LIBS}
        glog::glog
        # libmodbus
)


############################################################
# Add test exe
############################################################
set(TEST_MAIN Scanner_Server)
# set(TEST_MAIN pipe_scanner_forback)
add_executable(${TEST_MAIN} 
    src/${TEST_MAIN}.cpp
    include/${PROJECT_NAME}/Scanner_Server.h)
# target_include_directories(${PROJECT_NAME} PUBLIC 
#     $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
#     $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
# )
target_include_directories(${TEST_MAIN} PUBLIC 
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    ${CMAKE_SOURCE_DIR}/scanner_l/include/scanner_l
    ${OpenCV_INCLUDE_DIRS}
    ${GLOG_INCLUDE_PATH}
    # ${THIRD_PARTY_LIBRARY_DIR}/modbus/include
)
target_link_libraries(${TEST_MAIN} PRIVATE
    ${OpenCV_LIBS}
    glog::glog
    ScannerCtrl::scanner_l
    # libmodbus
    # serial::serial
)