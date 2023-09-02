cmake_minimum_required(VERSION 3.18)

project(NikonTiControl CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options("/Zc:__cplusplus")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include_directories("src")

################################
#  Dependencies
################################
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_PREFIX_PATH "C:/Qt/6.5.2/msvc2019_64/lib/cmake;${CMAKE_PREFIX_PATH}")
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}" REQUIRED)

find_package(absl REQUIRED)
add_compile_definitions(_HAS_DEPRECATED_RESULT_OF=1)

find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
get_target_property(Protobuf_PROTOC_EXECUTABLE protobuf::protoc IMPORTED_LOCATION_RELEASE)
find_package(TIFF REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc)
find_package(xtensor CONFIG REQUIRED)
find_package(hdf5 CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

find_package(gRPC CONFIG REQUIRED)
get_target_property(gRPC_CPP_PLUGIN_EXECUTABLE gRPC::grpc_cpp_plugin IMPORTED_LOCATION_RELEASE)

################################
#  Add Device Interface DLLs
################################

# ---------- MMCore API ----------
set(MM_DEVICEADAPTER_DIR "C:/Program Files/Micro-Manager-2.0")

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

# ---------- FLIR Spinnaker ----------
set(SPINNAKER_ROOT "C:/Program Files/FLIR Systems/Spinnaker")
include_directories("${SPINNAKER_ROOT}/include")
add_library(FLIR_Spinnaker SHARED IMPORTED)
set_target_properties(FLIR_Spinnaker PROPERTIES
    LINKER_LANGUAGE C
    IMPORTED_IMPLIB "${SPINNAKER_ROOT}/lib64/vs2015/Spinnaker_v140.lib"
    IMPORTED_LOCATION "${SPINNAKER_ROOT}/bin64/vs2015/Spinnaker_v140.dll"
)
install(FILES
    "${SPINNAKER_ROOT}/bin64/vs2015/Spinnaker_v140.dll"
    DESTINATION ${CMAKE_INSTALL_PREFIX}
)

################################
# Generated gRPC code
################################

# Proto file
get_filename_component(hw_proto "./src/api/api.proto" ABSOLUTE)
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
        --plugin=protoc-gen-grpc="${gRPC_CPP_PLUGIN_EXECUTABLE}"
        "${hw_proto}"
      DEPENDS "${hw_proto}")

## Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}/proto")

################################
# Generated tensorflow_serving gRPC code
################################
set (tf_serving_grpc_proto
    "tensorflow_serving/apis/prediction_service.proto"
)
set (tf_serving_proto
    "tensorflow_serving/apis/classification.proto"
    "tensorflow_serving/apis/get_model_metadata.proto"
    "tensorflow_serving/apis/inference.proto"
    "tensorflow_serving/apis/input.proto"
    "tensorflow_serving/apis/model.proto"
    "tensorflow_serving/apis/predict.proto"
    "tensorflow_serving/apis/regression.proto"
)

set (tf_proto
    "tensorflow/core/example/example.proto"
    "tensorflow/core/example/feature.proto"
    "tensorflow/core/framework/attr_value.proto"
    "tensorflow/core/framework/full_type.proto"
    "tensorflow/core/framework/function.proto"
    "tensorflow/core/framework/graph.proto"
    "tensorflow/core/framework/graph_debug_info.proto"
    "tensorflow/core/framework/node_def.proto"
    "tensorflow/core/framework/op_def.proto"
    "tensorflow/core/framework/resource_handle.proto"
    "tensorflow/core/framework/tensor_shape.proto"
    "tensorflow/core/framework/tensor.proto"
    "tensorflow/core/framework/types.proto"
    "tensorflow/core/framework/variable.proto"
    "tensorflow/core/framework/versions.proto"
    "tensorflow/core/protobuf/meta_graph.proto"
    "tensorflow/core/protobuf/saved_object_graph.proto"
    "tensorflow/core/protobuf/saver.proto"
    "tensorflow/core/protobuf/struct.proto"
    "tensorflow/core/protobuf/trackable_object_graph.proto"
)

foreach (proto_file ${tf_serving_grpc_proto})
    list (APPEND tf_serving_grpc_path "${CMAKE_CURRENT_SOURCE_DIR}/third_party/serving/${proto_file}")
    string(REGEX REPLACE "[.]proto$" ".grpc.pb.cc" proto_src ${proto_file})
    string(REGEX REPLACE "[.]proto$" ".grpc.pb.h" proto_hdr ${proto_file})
    list (APPEND tf_serving_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/proto/${proto_src}")
    list (APPEND tf_serving_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/proto/${proto_hdr}")
endforeach()

foreach (proto_file ${tf_serving_proto})
    list (APPEND tf_serving_grpc_path "${CMAKE_CURRENT_SOURCE_DIR}/third_party/serving/${proto_file}")
    string(REGEX REPLACE "[.]proto$" ".pb.cc" proto_src ${proto_file})
    string(REGEX REPLACE "[.]proto$" ".pb.h" proto_hdr ${proto_file})
    list (APPEND tf_serving_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/proto/${proto_src}")
    list (APPEND tf_serving_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/proto/${proto_hdr}")
endforeach()

foreach (proto_file ${tf_proto})
    list (APPEND tf_proto_path "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tensorflow/${proto_file}")
    string(REGEX REPLACE "[.]proto$" ".pb.cc" proto_src ${proto_file})
    string(REGEX REPLACE "[.]proto$" ".pb.h" proto_hdr ${proto_file})
    list (APPEND tf_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/proto/${proto_src}")
    list (APPEND tf_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/proto/${proto_hdr}")
endforeach()

add_custom_command(
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/serving"
      OUTPUT ${tf_serving_proto_srcs} ${tf_serving_proto_hdrs} ${tf_serving_grpc_srcs} ${tf_serving_grpc_hdrs}
      COMMAND ${Protobuf_PROTOC_EXECUTABLE}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}/proto"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}/proto"
        -I "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tensorflow"
        -I "${CMAKE_CURRENT_SOURCE_DIR}/third_party/serving"
        --plugin=protoc-gen-grpc="${gRPC_CPP_PLUGIN_EXECUTABLE}"
        ${tf_serving_grpc_proto}
        ${tf_serving_proto}
      DEPENDS ${tf_serving_grpc_path}
)

add_custom_command(
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tensorflow"
        OUTPUT ${tf_proto_srcs} ${tf_proto_hdrs}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --cpp_out "${CMAKE_CURRENT_BINARY_DIR}/proto"
          -I "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tensorflow"
          ${tf_proto}
        DEPENDS ${tf_proto_path}
)
  
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

    src/qt/devicecontrol_model.cpp
    src/qt/experimentcontrol_model.cpp
    src/qt/channelcontrol_model.cpp
    src/qt/samplemanager_model.cpp
    src/qt/imagemanager_model.cpp

    src/qt/setuppage.cpp
    src/qt/acquirepage.cpp
    src/qt/datapage.cpp

    src/qt/devicecontrol_view.cpp
    src/qt/acquisitioncontrol_view.cpp
    src/qt/samplemanager_view.cpp
    src/qt/imagemanager_view.cpp
    
    src/qt/ndimage_view.cpp
    src/qt/glimageview.cpp
    src/qt/glimageview.qrc

    src/qt/imagehistview.cpp
    src/qt/cmap/cmap_data.cpp

    src/qt/widgets/platesamplewidget.cpp
    
    third_party/QDarkStyleSheet/qdarkstyle/dark/darkstyle.qrc
)

add_executable(NikonTiControl
    src/main.cpp
    src/channel.cpp
    src/config.cpp
    src/eventstream.cpp
    src/logging.cpp
    src/experimentcontrol.cpp
    src/experimentdb.cpp
    ${version_srcs}
    ${QT_SOURCES}

    src/analysis/analysismanager.cpp
    src/analysis/segmentation.cpp
    src/sample/sample.cpp
    src/sample/samplemanager.cpp
    src/image/imagemanager.cpp
    src/image/imagedata.cpp
    src/image/imageutils.cpp
    src/image/ndimage.cpp
    src/task/channelcontrol.cpp
    src/task/live_view_task.cpp
    src/task/multi_channel_task.cpp
    
    src/device/device.cpp
    src/device/devicehub.cpp
    src/device/propertypath.cpp

    src/device/hamamatsu/hamamatsu_dcam.cpp
    src/device/nikon/mm_api.cpp
    src/device/nikon/nikon_ti_prop_info.cpp
    src/device/nikon/nikon_ti.cpp
    src/device/prior/prior_proscan_prop_info.cpp
    src/device/prior/prior_proscan.cpp
    src/device/flir/flir_spinnaker.cpp

    src/utils/time_utils.cpp
    src/utils/wmi.cpp
    src/utils/uuid.cpp
    src/utils/structarray.cpp
    src/utils/hdf5file.cpp
    src/utils/tifffile.cpp
    src/utils/tiff_stream.cpp
    src/utils/zipfile.cpp

    src/api/api_server.cpp
    ${hw_proto_srcs}
    ${hw_grpc_srcs}

    ${tf_serving_proto_srcs}
    ${tf_serving_proto_hdrs}
    ${tf_serving_grpc_srcs}
    ${tf_serving_grpc_hdrs}
    ${tf_proto_srcs}
    ${tf_proto_hdrs}
)

target_link_libraries(NikonTiControl PRIVATE
    wbemuuid # WMI
    ole32    # UUID

    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets

    absl::statusor
    fmt::fmt
    gRPC::grpc++
    gRPC::grpc++_reflection
    nlohmann_json::nlohmann_json
    unofficial::sqlite3::sqlite3
    protobuf::libprotobuf
    TIFF::TIFF
    opencv_core
    opencv_imgproc
    hdf5::hdf5-static
    xtensor
    xtensor::optimize
    ZLIB::ZLIB

    # Device Interface
    DCAMAPI
    VISA
    FLIR_Spinnaker
)

# --------------- Copy to binary dir for debugging ---------------
# Copy MMCore and device adapter dlls to build directory
add_custom_command(TARGET NikonTiControl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different 
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/MMCoreAPI/lib/MMCoreC.dll"
        "${MM_DEVICEADAPTER_DIR}/mmgr_dal_NikonTI.dll"
        "${SPINNAKER_ROOT}/bin64/vs2015/Spinnaker_v140.dll"
        "${CMAKE_CURRENT_BINARY_DIR}"
)

# Run windeployqt
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(WINDEPLOYQT_FLAG "--debug")
else()
    set(WINDEPLOYQT_FLAG "--release")
endif()

add_custom_command(TARGET NikonTiControl POST_BUILD
    # for installing
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/windeployqt"
    COMMAND "${WINDEPLOYQT_EXECUTABLE}" "${WINDEPLOYQT_FLAG}" --dir "${CMAKE_CURRENT_BINARY_DIR}/windeployqt" --no-translations "$<TARGET_FILE:NikonTiControl>"
    # for debugging
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/windeployqt/" "${CMAKE_CURRENT_BINARY_DIR}"
)

# ---------------  Install ---------------
# Install binary and runtime
install (TARGETS NikonTiControl
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/MMCoreAPI/lib/MMCoreC.dll"
    "${MM_DEVICEADAPTER_DIR}/mmgr_dal_NikonTI.dll"
    DESTINATION ${CMAKE_INSTALL_PREFIX}
)

# Install files from windeployqt
install(
    DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/windeployqt/" DESTINATION ${CMAKE_INSTALL_PREFIX}
)

# Install python API
install(
    DIRECTORY "python" DESTINATION ${CMAKE_INSTALL_PREFIX}
    FILES_MATCHING
    PATTERN "*.py"
    PATTERN "__pycache__" EXCLUDE
)

################################
# Create package
################################
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${GIT_TAG_VERSION}")

include(CPack)
