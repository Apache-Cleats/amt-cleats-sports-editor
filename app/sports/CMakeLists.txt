# AMT Cleats Sports Analysis Module
# Integration with Olive Video Editor Build System

cmake_minimum_required(VERSION 3.16)

# Sports Analysis Module Configuration
set(SPORTS_MODULE_NAME "amt_sports_analysis")
set(SPORTS_VERSION_MAJOR 1)
set(SPORTS_VERSION_MINOR 0)
set(SPORTS_VERSION_PATCH 0)

project(${SPORTS_MODULE_NAME} 
    VERSION ${SPORTS_VERSION_MAJOR}.${SPORTS_VERSION_MINOR}.${SPORTS_VERSION_PATCH}
    DESCRIPTION "AMT Cleats Sports Analysis Engine for Football Intelligence"
    LANGUAGES CXX
)

# C++ Standard Requirements
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build Configuration
option(ENABLE_SPORTS_ANALYSIS "Enable sports analysis features" ON)
option(ENABLE_TRIANGLE_DEFENSE "Enable Triangle Defense methodology" ON)
option(ENABLE_MEL_AI_INTEGRATION "Enable M.E.L. AI system integration" ON)
option(ENABLE_OPENCV_INTEGRATION "Enable OpenCV computer vision" ON)
option(BUILD_SPORTS_TESTS "Build sports analysis unit tests" OFF)

# Sports Analysis Sources
set(SPORTS_HEADERS
    sports_analysis_core.h
    formation_detector.h
    coaching_panel.h
)

set(SPORTS_SOURCES
    sports_analysis_core.cpp
    formation_detector.cpp
    coaching_panel.cpp
)

# Find Required Dependencies
if(ENABLE_OPENCV_INTEGRATION)
    find_package(OpenCV REQUIRED COMPONENTS
        core
        imgproc
        imgcodecs
        videoio
        objdetect
        dnn
    )
    if(OpenCV_FOUND)
        message(STATUS "OpenCV found: ${OpenCV_VERSION}")
        message(STATUS "OpenCV libs: ${OpenCV_LIBS}")
    else()
        message(FATAL_ERROR "OpenCV not found - required for formation detection")
    endif()
endif()

# Qt6 Integration (inherited from Olive)
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    OpenGL
)

# Threading Support
find_package(Threads REQUIRED)

# Create Sports Analysis Library
if(ENABLE_SPORTS_ANALYSIS)
    add_library(${SPORTS_MODULE_NAME} STATIC
        ${SPORTS_HEADERS}
        ${SPORTS_SOURCES}
    )
    
    # Set target properties
    set_target_properties(${SPORTS_MODULE_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        VERSION ${PROJECT_VERSION}
        SOVERSION ${SPORTS_VERSION_MAJOR}
    )
    
    # Include directories
    target_include_directories(${SPORTS_MODULE_NAME} 
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    # Link libraries
    target_link_libraries(${SPORTS_MODULE_NAME}
        PUBLIC
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::OpenGL
        PRIVATE
            Threads::Threads
    )
    
    # OpenCV Integration
    if(ENABLE_OPENCV_INTEGRATION AND OpenCV_FOUND)
        target_link_libraries(${SPORTS_MODULE_NAME}
            PRIVATE
                ${OpenCV_LIBS}
        )
        target_include_directories(${SPORTS_MODULE_NAME}
            PRIVATE
                ${OpenCV_INCLUDE_DIRS}
        )
        target_compile_definitions(${SPORTS_MODULE_NAME}
            PRIVATE
                ENABLE_OPENCV_INTEGRATION
        )
    endif()
    
    # Compiler definitions
    target_compile_definitions(${SPORTS_MODULE_NAME}
        PUBLIC
            AMT_SPORTS_VERSION_MAJOR=${SPORTS_VERSION_MAJOR}
            AMT_SPORTS_VERSION_MINOR=${SPORTS_VERSION_MINOR}
            AMT_SPORTS_VERSION_PATCH=${SPORTS_VERSION_PATCH}
    )
    
    if(ENABLE_TRIANGLE_DEFENSE)
        target_compile_definitions(${SPORTS_MODULE_NAME}
            PUBLIC
                ENABLE_TRIANGLE_DEFENSE
        )
    endif()
    
    if(ENABLE_MEL_AI_INTEGRATION)
        target_compile_definitions(${SPORTS_MODULE_NAME}
            PUBLIC
                ENABLE_MEL_AI_INTEGRATION
        )
    endif()
    
    # Compiler flags for optimization
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${SPORTS_MODULE_NAME}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -O3
                -march=native
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${SPORTS_MODULE_NAME}
            PRIVATE
                /W4
                /O2
        )
    endif()
    
    # Platform-specific configurations
    if(WIN32)
        target_compile_definitions(${SPORTS_MODULE_NAME}
            PRIVATE
                WIN32_LEAN_AND_MEAN
                NOMINMAX
        )
    endif()
    
    if(APPLE)
        target_compile_definitions(${SPORTS_MODULE_NAME}
            PRIVATE
                MACOS_BUILD
        )
    endif()
    
    if(UNIX AND NOT APPLE)
        target_compile_definitions(${SPORTS_MODULE_NAME}
            PRIVATE
                LINUX_BUILD
        )
    endif()
    
    message(STATUS "AMT Sports Analysis Module configured successfully")
    message(STATUS "  - Triangle Defense: ${ENABLE_TRIANGLE_DEFENSE}")
    message(STATUS "  - M.E.L. AI Integration: ${ENABLE_MEL_AI_INTEGRATION}")
    message(STATUS "  - OpenCV Integration: ${ENABLE_OPENCV_INTEGRATION}")
endif()

# Sports Analysis Tests
if(BUILD_SPORTS_TESTS AND ENABLE_SPORTS_ANALYSIS)
    enable_testing()
    
    # Create test executable
    add_executable(sports_tests
        tests/test_sports_analysis.cpp
        tests/test_formation_detector.cpp
        tests/test_triangle_defense.cpp
    )
    
    target_link_libraries(sports_tests
        PRIVATE
            ${SPORTS_MODULE_NAME}
            Qt6::Test
    )
    
    # Add tests
    add_test(
        NAME sports_analysis_tests
        COMMAND sports_tests
    )
    
    message(STATUS "Sports Analysis Tests enabled")
endif()

# Integration with Olive App
if(TARGET olive_app)
    # Link sports module to main Olive application
    target_link_libraries(olive_app
        PRIVATE
            ${SPORTS_MODULE_NAME}
    )
    
    # Add sports include directories to Olive
    target_include_directories(olive_app
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    message(STATUS "Sports Analysis Module linked to Olive App")
else()
    message(STATUS "Olive App target not found - sports module built standalone")
endif()

# Installation Rules
if(ENABLE_SPORTS_ANALYSIS)
    install(TARGETS ${SPORTS_MODULE_NAME}
        EXPORT AMTSportsTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
    
    install(FILES ${SPORTS_HEADERS}
        DESTINATION include/amt/sports
    )
    
    # Export configuration
    install(EXPORT AMTSportsTargets
        FILE AMTSportsTargets.cmake
        NAMESPACE AMT::
        DESTINATION lib/cmake/AMTSports
    )
    
    # Create config file
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        AMTSportsConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/AMTSportsConfigVersion.cmake
        DESTINATION lib/cmake/AMTSports
    )
endif()

# Debug Information
message(STATUS "=== AMT Cleats Sports Analysis Module Configuration ===")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CXX Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Sports Analysis: ${ENABLE_SPORTS_ANALYSIS}")
message(STATUS "Triangle Defense: ${ENABLE_TRIANGLE_DEFENSE}")
message(STATUS "M.E.L. AI Integration: ${ENABLE_MEL_AI_INTEGRATION}")
message(STATUS "OpenCV Integration: ${ENABLE_OPENCV_INTEGRATION}")
message(STATUS "Build Tests: ${BUILD_SPORTS_TESTS}")
message(STATUS "========================================================")
