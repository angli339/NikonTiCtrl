cmake_minimum_required(VERSION 3.18)

project(NikonTiControl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include_directories("src")

################################
#  Dependencies
################################
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_PREFIX_PATH "C:/Qt/5.15.2/msvc2019_64/lib/cmake;${CMAKE_PREFIX_PATH}")
find_package(Qt5 COMPONENTS Widgets Concurrent Charts REQUIRED)

get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}" REQUIRED)

find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(TIFF REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin REQUIRED)

################################
#  Add Device Interface DLLs
################################

# ---------- MMCore API ----------
set(MICROMANAGER14_DIR "C:/Program Files/Micro-Manager-1.4")
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/MMCoreAPI/lib/MMCoreC.dll"
    "${MICROMANAGER14_DIR}/mmgr_dal_NikonTI.dll"
    DESTINATION .
)

# ---------- VISA ----------
include_directories("C:/Program Files/IVI Foundation/VISA/Win64/include")
add_library(VISA SHARED IMPORTED)
set_target_properties(VISA PROPERTIES
    LINKER_LANGUAGE C
    IMPORTED_IMPLIB "C:/Program Files/IVI Foundation/VISA/Win64/Lib_x64/msc/visa64.lib"
    IMPORTED_LOCATION "C:/Windows/System32/visa64.dll"
)

# ---------- Hamamatsu DCAM SDK ----------
include_directories(third_party/dcamsdk4/inc)
add_library(DCAMAPI SHARED IMPORTED)
set_target_properties(DCAMAPI PROPERTIES
    LINKER_LANGUAGE C
    IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/third_party/dcamsdk4/lib/win64/dcamapi.lib"
    IMPORTED_LOCATION "C:/Windows/System32/DCAMAPI.DLL"
)

################################
# Generated gRPC code
################################

# Proto file
get_filename_component(hw_proto "./src/api.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)

# Generated sources
set(hw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/proto/api.pb.cc")
set(hw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/proto/api.pb.h")
set(hw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/proto/api.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/proto/api.grpc.pb.h")

add_custom_command(
      OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
      COMMAND ${Protobuf_PROTOC_EXECUTABLE}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}/proto"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}/proto"
        -I "${hw_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${hw_proto}"
      DEPENDS "${hw_proto}")

## Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}/proto")

################################
#  Find version
################################
find_package(Git REQUIRED)
# Find tags
execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --match "v*"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG_VERSION
    ERROR_VARIABLE GIT_TAG_VERSION_ERROR
    RESULT_VARIABLE GIT_TAG_VERSION_ERROR_CODE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(GIT_TAG_VERSION_ERROR_CODE)
    # Find version from tags
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_HASH
        ERROR_VARIABLE GIT_HASH_ERROR
        RESULT_VARIABLE GIT_HASH_ERROR_CODE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(GIT_HASH_ERROR_CODE)
        message(WARNING "Failed to get git hash.")
    else()
        set(GIT_TAG_VERSION "v0.0.0-${GIT_HASH}")
    endif()
endif()

message(STATUS "---------- summary ----------")
message(STATUS "${CMAKE_PROJECT_NAME} Version: ${GIT_TAG_VERSION}")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/version.cpp" @ONLY)
set(version_srcs "${CMAKE_CURRENT_BINARY_DIR}/version.cpp")

################################
# Target
################################

set(QT_SOURCES
    src/qt/mainwindow.cpp
    src/qt/mainwindow.h
    src/qt/mainwindow.ui
    src/qt/nikon_control.qrc
    src/qt/glimageviewer.cpp
    src/qt/cmap_data.cpp
)

add_executable(NikonTiControl
    src/main.cpp
    ${version_srcs}
    ${QT_SOURCES}

    src/image.cpp
    src/logger.cpp
    src/devicecontrol.cpp
    src/taskcontrol.cpp
    src/datamanager.cpp
    src/propertystatus.cpp
    src/config.cpp

    # Device Interface
    src/device/mm_api.cpp
    src/device/nikon_ti.cpp
    src/device/nikon_ti_property.cpp
    src/device/prior_proscan.cpp
    src/device/prior_proscan_property.cpp
    src/device/hamamatsu_dcam.cpp

    # Device Util
    src/utils/time_utils.cpp
    src/utils/string_utils.cpp
    src/utils/wmi.cpp
    src/utils/devnotify.cpp

    # gRPC API
    src/api_server.cpp
    ${hw_proto_srcs}
    ${hw_grpc_srcs}
)

install (TARGETS NikonTiControl
    RUNTIME DESTINATION .
)

# Copy MMCore and device adapter dlls to build directory
add_custom_command(TARGET NikonTiControl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy 
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/MMCoreAPI/lib/MMCoreC.dll"
        "${MICROMANAGER14_DIR}/mmgr_dal_NikonTI.dll"
        "${CMAKE_CURRENT_BINARY_DIR}"
)

# Run windeployqt
add_custom_command(TARGET NikonTiControl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/windeployqt"
    COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release --dir "${CMAKE_CURRENT_BINARY_DIR}/windeployqt" --no-translations "$<TARGET_FILE:NikonTiControl>"
    COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release --dir "${CMAKE_CURRENT_BINARY_DIR}" --no-translations "$<TARGET_FILE:NikonTiControl>"
)

# Copy files from windeployqt during installation
install(
    DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/windeployqt/" DESTINATION .
)

target_link_libraries(NikonTiControl PRIVATE
    wbemuuid # WMI

    Qt5::Widgets
    Qt5::Concurrent
    Qt5::Charts

    fmt::fmt
    TIFF::TIFF
    nlohmann_json::nlohmann_json
    protobuf::libprotobuf
    gRPC::grpc++
    gRPC::grpc++_reflection

    # Device Interface
    DCAMAPI
    VISA
)

################################
# Create package
################################
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${GIT_TAG_VERSION}")

include(CPack)
