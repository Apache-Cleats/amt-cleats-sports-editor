#
# Apache-Cleats Sports Editor
# Copyright (C) 2024 AnalyzeMyTeam
#
# Sports Integration Module CMakeLists
# Builds Triangle Defense, M.E.L. pipeline, and Superset integration
#

# Minimum CMake version
cmake_minimum_required(VERSION 3.16)

# Sports module configuration
set(SPORTS_MODULE_NAME "apache_cleats_sports")
set(SPORTS_VERSION_MAJOR 2)
set(SPORTS_VERSION_MINOR 0)
set(SPORTS_VERSION_PATCH 0)

# Module sources
set(SPORTS_SOURCES
    superset_panel.cpp
    superset_panel.h
    kafka_publisher.cpp
    kafka_publisher.h
    triangle_defense_sync.cpp
    triangle_defense_sync.h
    minio_client.cpp
    minio_client.h
    sports_integration.cpp
    sports_integration.h
    video_timeline_sync.cpp
    video_timeline_sync.h
    formation_overlay.cpp
    formation_overlay.h
    coaching_alert_widget.cpp
    coaching_alert_widget.h
)

# Qt6 components required for sports integration
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    WebEngine
    WebEngineWidgets
    WebChannel
    Network
    Sql
    Multimedia
    MultimediaWidgets
    Concurrent
)

# Third-party dependencies
find_package(PkgConfig REQUIRED)

# FFmpeg for video processing
pkg_check_modules(FFMPEG REQUIRED
    libavformat>=58.0.0
    libavcodec>=58.0.0
    libavutil>=56.0.0
    libswscale>=5.0.0
    libswresample>=3.0.0
)

# librdkafka for Kafka integration
pkg_check_modules(RDKAFKA REQUIRED rdkafka>=1.6.0)

# OpenSSL for MinIO authentication
find_package(OpenSSL REQUIRED)

# JSON library
find_package(nlohmann_json REQUIRED)

# SQLite3 for local caching
find_package(SQLite3 REQUIRED)

# WebRTC for real-time communication (optional)
find_package(PkgConfig)
pkg_check_modules(WEBRTC libwebrtc)

# Triangle Defense AI models (optional)
find_package(PkgConfig)
pkg_check_modules(TENSORFLOW tensorflow-lite)
pkg_check_modules(ONNX onnxruntime)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/app
    ${FFMPEG_INCLUDE_DIRS}
    ${RDKAFKA_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

# Compiler definitions
add_definitions(
    -DSPORTS_MODULE_VERSION="${SPORTS_VERSION_MAJOR}.${SPORTS_VERSION_MINOR}.${SPORTS_VERSION_PATCH}"
    -DTRIANGLE_DEFENSE_ENABLED=1
    -DMEL_PIPELINE_ENABLED=1
    -DSUPERSET_INTEGRATION_ENABLED=1
    -DMINIO_STORAGE_ENABLED=1
    -DKAFKA_EVENTS_ENABLED=1
)

# Enable specific features based on available dependencies
if(WEBRTC_FOUND)
    add_definitions(-DWEBRTC_ENABLED=1)
    message(STATUS "WebRTC support enabled for real-time communication")
else()
    message(STATUS "WebRTC not found - real-time features will use WebSocket fallback")
endif()

if(TENSORFLOW_FOUND)
    add_definitions(-DTENSORFLOW_AI_ENABLED=1)
    message(STATUS "TensorFlow Lite support enabled for AI inference")
endif()

if(ONNX_FOUND)
    add_definitions(-DONNX_AI_ENABLED=1)
    message(STATUS "ONNX Runtime support enabled for AI inference")
endif()

# Conditional compilation for different platforms
if(WIN32)
    add_definitions(-DWINDOWS_PLATFORM=1)
    # Windows-specific WebEngine settings
    add_definitions(-DWEBENGINE_WINDOWS_TWEAKS=1)
elseif(APPLE)
    add_definitions(-DMACOS_PLATFORM=1)
    # macOS-specific optimizations
    add_definitions(-DMACOS_METAL_ACCELERATION=1)
elseif(UNIX)
    add_definitions(-DLINUX_PLATFORM=1)
    # Linux-specific optimizations
    add_definitions(-DLINUX_VA_API_ENABLED=1)
endif()

# Sports module library
add_library(${SPORTS_MODULE_NAME} STATIC ${SPORTS_SOURCES})

# Target properties
set_target_properties(${SPORTS_MODULE_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    VERSION ${SPORTS_VERSION_MAJOR}.${SPORTS_VERSION_MINOR}.${SPORTS_VERSION_PATCH}
    SOVERSION ${SPORTS_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
)

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${SPORTS_MODULE_NAME} PRIVATE
        -O3
        -march=native
        -mtune=native
        -ffast-math
        -funroll-loops
        -DNDEBUG
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${SPORTS_MODULE_NAME} PRIVATE
        -O3
        -march=native
        -mtune=native
        -ffast-math
        -funroll-loops
        -DNDEBUG
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${SPORTS_MODULE_NAME} PRIVATE
        /O2
        /arch:AVX2
        /fp:fast
        /DNDEBUG
    )
endif()

# Link Qt6 libraries
target_link_libraries(${SPORTS_MODULE_NAME}
    Qt6::Core
    Qt6::Widgets
    Qt6::WebEngine
    Qt6::WebEngineWidgets
    Qt6::WebChannel
    Qt6::Network
    Qt6::Sql
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Concurrent
)

# Link FFmpeg libraries
target_link_libraries(${SPORTS_MODULE_NAME}
    ${FFMPEG_LIBRARIES}
)

# Link librdkafka
target_link_libraries(${SPORTS_MODULE_NAME}
    ${RDKAFKA_LIBRARIES}
)

# Link OpenSSL
target_link_libraries(${SPORTS_MODULE_NAME}
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Link JSON library
target_link_libraries(${SPORTS_MODULE_NAME}
    nlohmann_json::nlohmann_json
)

# Link SQLite3
target_link_libraries(${SPORTS_MODULE_NAME}
    SQLite::SQLite3
)

# Optional dependencies
if(WEBRTC_FOUND)
    target_link_libraries(${SPORTS_MODULE_NAME} ${WEBRTC_LIBRARIES})
endif()

if(TENSORFLOW_FOUND)
    target_link_libraries(${SPORTS_MODULE_NAME} ${TENSORFLOW_LIBRARIES})
endif()

if(ONNX_FOUND)
    target_link_libraries(${SPORTS_MODULE_NAME} ${ONNX_LIBRARIES})
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(${SPORTS_MODULE_NAME}
        ws2_32
        winmm
        mfplat
        mfreadwrite
        mfuuid
        ole32
        oleaut32
    )
elseif(APPLE)
    target_link_libraries(${SPORTS_MODULE_NAME}
        "-framework Foundation"
        "-framework AVFoundation"
        "-framework CoreMedia"
        "-framework VideoToolbox"
        "-framework Metal"
        "-framework MetalKit"
    )
elseif(UNIX)
    target_link_libraries(${SPORTS_MODULE_NAME}
        pthread
        dl
        va
        va-drm
        X11
    )
endif()

# Compiler flags for different build types
target_compile_definitions(${SPORTS_MODULE_NAME} PRIVATE
    $<$<CONFIG:Debug>:DEBUG_BUILD=1>
    $<$<CONFIG:Debug>:ENABLE_DETAILED_LOGGING=1>
    $<$<CONFIG:Debug>:TRIANGLE_DEFENSE_DEBUG=1>
    $<$<CONFIG:Release>:RELEASE_BUILD=1>
    $<$<CONFIG:Release>:OPTIMIZED_PERFORMANCE=1>
    $<$<CONFIG:RelWithDebInfo>:RELEASE_WITH_DEBUG_INFO=1>
    $<$<CONFIG:MinSizeRel>:MINIMAL_SIZE_BUILD=1>
)

# Custom build options
option(ENABLE_SPORTS_TELEMETRY "Enable telemetry and analytics reporting" OFF)
option(ENABLE_EXPERIMENTAL_FEATURES "Enable experimental Triangle Defense features" OFF)
option(ENABLE_GPU_ACCELERATION "Enable GPU acceleration for video processing" ON)
option(ENABLE_CLOUD_SYNC "Enable cloud synchronization features" ON)
option(BUILD_SPORTS_TESTS "Build sports module unit tests" OFF)
option(BUILD_SPORTS_BENCHMARKS "Build performance benchmarks" OFF)

if(ENABLE_SPORTS_TELEMETRY)
    add_definitions(-DSPORTS_TELEMETRY_ENABLED=1)
    message(STATUS "Sports telemetry enabled")
endif()

if(ENABLE_EXPERIMENTAL_FEATURES)
    add_definitions(-DEXPERIMENTAL_FEATURES_ENABLED=1)
    message(STATUS "Experimental Triangle Defense features enabled")
endif()

if(ENABLE_GPU_ACCELERATION)
    add_definitions(-DGPU_ACCELERATION_ENABLED=1)
    
    # Try to find CUDA
    find_package(CUDA QUIET)
    if(CUDA_FOUND)
        add_definitions(-DCUDA_ACCELERATION_ENABLED=1)
        target_link_libraries(${SPORTS_MODULE_NAME} ${CUDA_LIBRARIES})
        message(STATUS "CUDA GPU acceleration enabled")
    endif()
    
    # Try to find OpenCL
    find_package(OpenCL QUIET)
    if(OpenCL_FOUND)
        add_definitions(-DOPENCL_ACCELERATION_ENABLED=1)
        target_link_libraries(${SPORTS_MODULE_NAME} OpenCL::OpenCL)
        message(STATUS "OpenCL GPU acceleration enabled")
    endif()
endif()

if(ENABLE_CLOUD_SYNC)
    add_definitions(-DCLOUD_SYNC_ENABLED=1)
    message(STATUS "Cloud synchronization features enabled")
endif()

# Resource files and configurations
set(SPORTS_RESOURCES
    resources/triangle_defense_models.qrc
    resources/formation_templates.qrc
    resources/coaching_icons.qrc
    resources/mel_pipeline_configs.qrc
)

# Process Qt resource files
qt6_add_resources(${SPORTS_MODULE_NAME} ${SPORTS_RESOURCES})

# Generate configuration header
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/sports_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/sports_config.h
    @ONLY
)

# Installation rules
install(TARGETS ${SPORTS_MODULE_NAME}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    superset_panel.h
    kafka_publisher.h
    triangle_defense_sync.h
    minio_client.h
    sports_integration.h
    ${CMAKE_CURRENT_BINARY_DIR}/sports_config.h
    DESTINATION include/apache_cleats/sports
)

# Generate pkg-config file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/apache_cleats_sports.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/apache_cleats_sports.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/apache_cleats_sports.pc
    DESTINATION lib/pkgconfig
)

# Unit tests (optional)
if(BUILD_SPORTS_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks (optional)
if(BUILD_SPORTS_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Documentation generation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
        ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        @ONLY
    )
    
    add_custom_target(sports_docs
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating Sports Module API documentation with Doxygen"
        VERBATIM
    )
endif()

# Code analysis tools
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if(CLANG_TIDY_EXE)
    set_target_properties(${SPORTS_MODULE_NAME} PROPERTIES
        CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-checks=-*,readability-*,performance-*,modernize-*"
    )
endif()

# Custom targets for development
add_custom_target(sports_format
    COMMAND clang-format -i ${SPORTS_SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Formatting sports module source code"
)

add_custom_target(sports_lint
    COMMAND cppcheck --enable=all --std=c++17 ${SPORTS_SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running static analysis on sports module"
)

# Performance profiling targets
add_custom_target(sports_profile_build
    COMMAND ${CMAKE_COMMAND} --build . --target ${SPORTS_MODULE_NAME} -- -fprofile-generate
    COMMENT "Building sports module with profiling instrumentation"
)

add_custom_target(sports_profile_use
    COMMAND ${CMAKE_COMMAND} --build . --target ${SPORTS_MODULE_NAME} -- -fprofile-use
    COMMENT "Building sports module with profile-guided optimization"
)

# Memory sanitizer builds
add_custom_target(sports_asan
    COMMAND ${CMAKE_COMMAND} -DCMAKE_CXX_FLAGS="-fsanitize=address -g" ..
    COMMENT "Configure build with AddressSanitizer"
)

add_custom_target(sports_tsan
    COMMAND ${CMAKE_COMMAND} -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" ..
    COMMENT "Configure build with ThreadSanitizer"
)

add_custom_target(sports_msan
    COMMAND ${CMAKE_COMMAND} -DCMAKE_CXX_FLAGS="-fsanitize=memory -g" ..
    COMMENT "Configure build with MemorySanitizer"
)

# Packaging configuration
set(CPACK_PACKAGE_NAME "Apache-Cleats-Sports")
set(CPACK_PACKAGE_VERSION "${SPORTS_VERSION_MAJOR}.${SPORTS_VERSION_MINOR}.${SPORTS_VERSION_PATCH}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Apache Cleats Sports Integration Module")
set(CPACK_PACKAGE_VENDOR "AnalyzeMyTeam")
set(CPACK_PACKAGE_CONTACT "support@analyzemy.team")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# Platform-specific packaging
if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "Apache Cleats Sports Module")
    set(CPACK_NSIS_HELP_LINK "https://docs.analyzemy.team")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://analyzemy.team")
    set(CPACK_NSIS_CONTACT "support@analyzemy.team")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ")
    set(CPACK_DMG_VOLUME_NAME "Apache Cleats Sports")
    set(CPACK_DMG_FORMAT "UDBZ")
elseif(UNIX)
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "AnalyzeMyTeam <support@analyzemy.team>")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6, libqt6widgets6, libqt6webengine6, libavformat58, librdkafka1")
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0")
    set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
endif()

include(CPack)

# Status messages
message(STATUS "=== Apache Cleats Sports Module Configuration ===")
message(STATUS "Version: ${SPORTS_VERSION_MAJOR}.${SPORTS_VERSION_MINOR}.${SPORTS_VERSION_PATCH}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Qt6 version: ${Qt6_VERSION}")
message(STATUS "FFmpeg found: ${FFMPEG_FOUND}")
message(STATUS "Kafka found: ${RDKAFKA_FOUND}")
message(STATUS "OpenSSL found: ${OPENSSL_FOUND}")
message(STATUS "GPU acceleration: ${ENABLE_GPU_ACCELERATION}")
message(STATUS "Cloud sync: ${ENABLE_CLOUD_SYNC}")
message(STATUS "Build tests: ${BUILD_SPORTS_TESTS}")
message(STATUS "Build benchmarks: ${BUILD_SPORTS_BENCHMARKS}")
message(STATUS "==============================================")

# Dependency version checking
if(Qt6_VERSION VERSION_LESS "6.2.0")
    message(WARNING "Qt6 version ${Qt6_VERSION} may have limited WebEngine support")
endif()

if(FFMPEG_VERSION VERSION_LESS "4.0")
    message(WARNING "FFmpeg version ${FFMPEG_VERSION} may have compatibility issues")
endif()

if(RDKAFKA_VERSION VERSION_LESS "1.6.0")
    message(WARNING "librdkafka version ${RDKAFKA_VERSION} may lack some features")
endif()

# Build summary
message(STATUS "Sports module will be built with the following features:")
message(STATUS "  - Triangle Defense real-time analysis")
message(STATUS "  - M.E.L. pipeline integration")
message(STATUS "  - Superset dashboard embedding")
message(STATUS "  - MinIO video storage")
message(STATUS "  - Kafka event streaming")
message(STATUS "  - Video timeline synchronization")
message(STATUS "  - Formation overlay rendering")
message(STATUS "  - Coaching alert system")
