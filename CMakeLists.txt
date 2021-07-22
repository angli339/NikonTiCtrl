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
# find windeployqt
get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}" REQUIRED)
message(STATUS "---------- Qt5 ----------")
message(STATUS "Qt5 Widgets: Version ${Qt5Widgets_VERSION}")
message(STATUS "windeployqt: ${WINDEPLOYQT_EXECUTABLE}")

# ---------- MSYS2 ----------
# provides binary packages
#     * tiff: mingw-w64-x86_64-libtiff
#     * protobuf: mingw-w64-x86_64-protobuf
#     * gRPC: mingw-w64-x86_64-grpc
# along with their dependencies
set(MSYS_PATH "C:/msys64/mingw64")
set(MSYS_BIN_PATH "${MSYS_PATH}/bin")
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${MSYS_PATH})

# ---------- libtiff ----------
message(STATUS "---------- libtiff ----------")
find_package(TIFF REQUIRED)
message(STATUS "libtiff include dir: ${TIFF_INCLUDE_DIRS}")
message(STATUS "libtiff libraries: ${TIFF_LIBRARIES}")
install(FILES
    "${MSYS_BIN_PATH}/libtiff-5.dll"
    "${MSYS_BIN_PATH}/libdeflate.dll"
    "${MSYS_BIN_PATH}/liblzma-5.dll"
    "${MSYS_BIN_PATH}/libjbig-0.dll"
    "${MSYS_BIN_PATH}/libjpeg-8.dll"
    "${MSYS_BIN_PATH}/libwebp-7.dll"
    "${MSYS_BIN_PATH}/libzstd.dll"
    "${MSYS_BIN_PATH}/zlib1.dll"
    DESTINATION .
)

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
install(FILES
    "${MSYS_BIN_PATH}/libprotobuf.dll"
    "${MSYS_BIN_PATH}/zlib1.dll"
    DESTINATION .
)

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
install(FILES
    "${MSYS_BIN_PATH}/libgrpc++.dll"
    "${MSYS_BIN_PATH}/libabsl_bad_optional_access.dll"
    "${MSYS_BIN_PATH}/libabsl_base.dll"
    "${MSYS_BIN_PATH}/libabsl_cord.dll"
    "${MSYS_BIN_PATH}/libabsl_int128.dll"
    "${MSYS_BIN_PATH}/libabsl_malloc_internal.dll"
    "${MSYS_BIN_PATH}/libabsl_raw_logging_internal.dll"
    "${MSYS_BIN_PATH}/libabsl_spinlock_wait.dll"
    "${MSYS_BIN_PATH}/libabsl_stacktrace.dll"
    "${MSYS_BIN_PATH}/libabsl_status.dll"
    "${MSYS_BIN_PATH}/libabsl_statusor.dll"
    "${MSYS_BIN_PATH}/libabsl_strings.dll"
    "${MSYS_BIN_PATH}/libabsl_strings_internal.dll"
    "${MSYS_BIN_PATH}/libabsl_str_format_internal.dll"
    "${MSYS_BIN_PATH}/libabsl_symbolize.dll"
    "${MSYS_BIN_PATH}/libabsl_synchronization.dll"
    "${MSYS_BIN_PATH}/libabsl_throw_delegate.dll"
    "${MSYS_BIN_PATH}/libabsl_time.dll"
    "${MSYS_BIN_PATH}/libabsl_time_zone.dll"
    "${MSYS_BIN_PATH}/libaddress_sorting.dll"
    "${MSYS_BIN_PATH}/libcares-4.dll"
    "${MSYS_BIN_PATH}/libcrypto-1_1-x64.dll"
    "${MSYS_BIN_PATH}/libgpr.dll"
    "${MSYS_BIN_PATH}/libgrpc.dll"
    "${MSYS_BIN_PATH}/libre2.dll"
    "${MSYS_BIN_PATH}/libssl-1_1-x64.dll"
    "${MSYS_BIN_PATH}/libupb.dll"
    "${MSYS_BIN_PATH}/zlib1.dll"
    DESTINATION .
)

################################
#  Add Subdirectories
################################

set(CMAKE_BUILD_TYPE_SAVE "${CMAKE_BUILD_TYPE}")
set(CMAKE_BUILD_TYPE RELEASE)
set(BUILD_SHARED_LIBS OFF)

# ---------- ghc::filesystem ----------
# gcc 8.1.0 does not yet support std::filesystem on Windows
message(STATUS "---------- ghc::filesystem ----------")
set(GHC_FILESYSTEM_WITH_INSTALL OFF CACHE INTERNAL "")
add_subdirectory(third_party/filesystem)

# ---------- fmt ----------
message(STATUS "---------- fmt ----------")
add_subdirectory(third_party/fmt)

# ---------- json ----------
message(STATUS "---------- json ----------")
set(JSON_Install OFF CACHE INTERNAL "")
set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(third_party/json)

set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE_SAVE}")

################################
#  Add Device Interface DLLs
################################

# ---------- MMCore API ----------
set(MICROMANAGER14_DIR "C:/Program Files/Micro-Manager-1.4")

include_directories(third_party/MMCoreAPI/MMCoreC)
add_library(MMCoreC UNKNOWN IMPORTED)
set_target_properties(MMCoreC PROPERTIES
    LINKER_LANGUAGE C
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/third_party/MMCoreAPI/lib/MMCoreC.dll"
)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/MMCoreAPI/lib/MMCoreC.dll"
    "${MICROMANAGER14_DIR}/mmgr_dal_NikonTI.dll"
    DESTINATION .
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

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/version.cpp" @ONLY)
set(version_srcs "${CMAKE_CURRENT_BINARY_DIR}/version.cpp")

################################
# Target
################################

add_executable(NikonTiControl
    main.cpp
    ${version_srcs}

    # UI
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    nikon_control.qrc

    image.cpp
    logger.cpp
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
    COMMAND "${WINDEPLOYQT_EXECUTABLE}" --dir "${CMAKE_CURRENT_BINARY_DIR}/windeployqt" --no-translations "$<TARGET_FILE:NikonTiControl>"
)

# Copy files from windeployqt during installation
install(
    DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/windeployqt/" DESTINATION .
)

target_link_libraries(NikonTiControl PRIVATE
    # Qt5
    Qt5::Widgets
    Qt5::Concurrent
    Qt5::Charts

    ghc_filesystem
    fmt::fmt
    TIFF::TIFF
    nlohmann_json::nlohmann_json

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

################################
# Create package
################################
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${GIT_TAG_VERSION}")

include(CPack)
