cmake_minimum_required(VERSION 3.5)

project(NikonTiControl LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

################################
#  Find Packages
################################

set(CMAKE_PREFIX_PATH "C:/Qt/5.15.2/mingw81_64")

# ---------- Qt5 ----------
find_package(QT NAMES Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt5 COMPONENTS Widgets Concurrent Charts REQUIRED)
message(STATUS "---------- Qt5 ----------")
message(STATUS "Qt5 Widgets: Version ${Qt5Widgets_VERSION}")

# ---------- MSYS2 ----------
# provides binary packages
#     * tiff: mingw-w64-x86_64-libtiff
#     * protobuf: mingw-w64-x86_64-protobuf
#     * gRPC: mingw-w64-x86_64-grpc
# along with their dependencies
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "C:/msys64/mingw64")

# ---------- libtiff ----------
message(STATUS "---------- libtiff ----------")
find_package(TIFF REQUIRED)
message(STATUS "libtiff include dir: ${TIFF_INCLUDE_DIRS}")
message(STATUS "libtiff libraries: ${TIFF_LIBRARIES}")

# ---------- SQLite ----------
message(STATUS "---------- SQLite ----------")
find_package(SQLite3 REQUIRED)
message(STATUS "SQLite version: ${SQLite3_VERSION}")

# ---------- Protobuf ----------
message(STATUS "---------- Protobuf ----------")
cmake_policy(SET CMP0077 NEW)
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")
if(CMAKE_CROSSCOMPILING)
  find_program(_PROTOBUF_PROTOC protoc)
else()
  set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
endif()

# ---------- gRPC ----------
message(STATUS "---------- gRPC ----------")
if(WIN32)
    # We have to set _WIN32_WINNT for gRPC
    if(${CMAKE_SYSTEM_VERSION} EQUAL 10) # Windows 10
        add_definitions(-D_WIN32_WINNT=0x0A00)
    elseif(${CMAKE_SYSTEM_VERSION} EQUAL 6.3) # Windows 8.1
        add_definitions(-D_WIN32_WINNT=0x0603)
    elseif(${CMAKE_SYSTEM_VERSION} EQUAL 6.2) # Windows 8
        add_definitions(-D_WIN32_WINNT=0x0602)
    elseif(${CMAKE_SYSTEM_VERSION} EQUAL 6.1) # Windows 7
        add_definitions(-D_WIN32_WINNT=0x0601)
    elseif(${CMAKE_SYSTEM_VERSION} EQUAL 6.0) # Windows Vista
        add_definitions(-D_WIN32_WINNT=0x0600)
    else() # Windows XP (5.1)
        add_definitions(-D_WIN32_WINNT=0x0501)
    endif()
endif()

cmake_policy(SET CMP0071 NEW)
find_package(Threads REQUIRED)
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")
if(CMAKE_CROSSCOMPILING)
  find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
else()
  set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
endif()
remove_definitions(-D_WIN32_WINNT)

################################
#  Add Subdirectories
################################

set(CMAKE_BUILD_TYPE_SAVE "${CMAKE_BUILD_TYPE}")
set(CMAKE_BUILD_TYPE RELEASE)

# ---------- fmt ----------
message(STATUS "---------- fmt ----------")
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(third_party/fmt)
set(BUILD_SHARED_LIBS ON)

# ---------- spdlog ----------
message(STATUS "---------- spdlog ----------")
set(SPDLOG_FMT_EXTERNAL ON CACHE INTERNAL "")
add_subdirectory(third_party/spdlog)

# ---------- json ----------
message(STATUS "---------- json ----------")
set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(third_party/json)

set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE_SAVE}")

################################
#  Add Device Interface DLLs
################################

# ---------- MMCore API ----------
include_directories(third_party/MMCoreAPI/MMCoreC)
add_library(MMCoreC UNKNOWN IMPORTED)
set_target_properties(MMCoreC PROPERTIES
    LINKER_LANGUAGE C
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/third_party/MMCoreAPI/lib/MMCoreC.dll"
)

# ---------- Hamamatsu DCAM SDK ----------
include_directories(third_party/dcamsdk4/inc)
add_library(DCAMAPI UNKNOWN IMPORTED)
set_target_properties(DCAMAPI PROPERTIES
    LINKER_LANGUAGE C
    IMPORTED_LOCATION "C:/Windows/System32/DCAMAPI.DLL"
)

# ---------- VISA ----------
include_directories("C:/Program Files/IVI Foundation/VISA/Win64/include")
add_library(VISA UNKNOWN IMPORTED)
set_target_properties(VISA PROPERTIES
    LINKER_LANGUAGE C
    IMPORTED_LOCATION "C:/Windows/System32/visa64.dll"
)

################################
# Generated gRPC code
################################

# Proto file
get_filename_component(hw_proto "./api.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)

# Generated sources
set(hw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/api.pb.cc")
set(hw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/api.pb.h")
set(hw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/api.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/api.grpc.pb.h")

add_custom_command(
      OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${hw_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${hw_proto}"
      DEPENDS "${hw_proto}")

## Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

################################
# Target
################################

add_executable(NikonTiControl
    main.cpp

    # UI
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    nikon_control.qrc

    image.cpp
    logging.cpp
    devicecontrol.cpp
    taskcontrol.cpp
    datamanager.cpp
    glimageviewer.cpp
    cmap_data.cpp
    propertystatus.cpp
    config.cpp

    # Device Interface
    device/nikon_ti.cpp
    device/nikon_ti_property.cpp
    device/prior_proscan.cpp
    device/prior_proscan_property.cpp
    device/hamamatsu_dcam.cpp
    device/trigger_controller.cpp

    # Device Util
    utils/time_utils.cpp
    utils/string_utils.cpp
    utils/wmi.cpp
    utils/devnotify.cpp

    # gRPC API
    api_server.cpp
    ${hw_proto_srcs}
    ${hw_grpc_srcs}
)

target_include_directories(NikonTiControl PUBLIC
    third_party/opencv/modules/core/include
    third_party/opencv/modules/imgproc/include
)

target_link_libraries(NikonTiControl PRIVATE
    # Qt5
    Qt5::Widgets
    Qt5::Concurrent
    Qt5::Charts

    fmt::fmt
    spdlog::spdlog
    TIFF::TIFF
    nlohmann_json::nlohmann_json
    SQLite::SQLite3

    # Device Interface
    MMCoreC
    DCAMAPI
    VISA

    # gRPC
    protobuf::libprotobuf
    gRPC::grpc++
    gRPC::grpc++_reflection

    # WMI
    wbemuuid
)
