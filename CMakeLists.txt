cmake_minimum_required(VERSION 3.12)

set(MAIN_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../main" CACHE PATH
    "Path to the main repository source tree")
set(MAIN_BUILD_DIR "" CACHE PATH
    "Path to the configured main CMake build tree whose libraries Analyzer will use")

get_filename_component(MAIN_SOURCE_DIR "${MAIN_SOURCE_DIR}" ABSOLUTE)
if(NOT MAIN_BUILD_DIR)
    message(FATAL_ERROR
        "MAIN_BUILD_DIR is required. Configure main first, then pass its build directory with "
        "-DMAIN_BUILD_DIR=/path/to/main-build.")
endif()
get_filename_component(MAIN_BUILD_DIR "${MAIN_BUILD_DIR}" ABSOLUTE)

if(NOT EXISTS "${MAIN_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "MAIN_SOURCE_DIR is not a main source tree: ${MAIN_SOURCE_DIR}")
endif()
if(NOT EXISTS "${MAIN_BUILD_DIR}/CMakeCache.txt")
    message(FATAL_ERROR "MAIN_BUILD_DIR is not a configured CMake build tree: ${MAIN_BUILD_DIR}")
endif()

load_cache("${MAIN_BUILD_DIR}" READ_WITH_PREFIX MAIN_CACHE_
    CMAKE_BUILD_TYPE
    CMAKE_CXX_COMPILER
    CMAKE_PREFIX_PATH
    Qt5_DIR)

if(NOT CMAKE_BUILD_TYPE AND MAIN_CACHE_CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "${MAIN_CACHE_CMAKE_BUILD_TYPE}" CACHE STRING
        "Analyzer build type; must match main" FORCE)
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER AND MAIN_CACHE_CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER "${MAIN_CACHE_CMAKE_CXX_COMPILER}" CACHE FILEPATH
        "C++ compiler; defaults to main's compiler")
endif()
if(NOT CMAKE_PREFIX_PATH AND MAIN_CACHE_CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "${MAIN_CACHE_CMAKE_PREFIX_PATH}" CACHE PATH
        "Dependency prefix; defaults to main's prefix")
endif()
if(NOT Qt5_DIR AND MAIN_CACHE_Qt5_DIR)
    set(Qt5_DIR "${MAIN_CACHE_Qt5_DIR}" CACHE PATH
        "Qt 5 package directory; defaults to main's Qt package")
endif()

project(analyzer LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Analyzer's standalone CMake integration currently supports Linux only")
endif()
if(CMAKE_CONFIGURATION_TYPES)
    message(FATAL_ERROR "Analyzer requires a single-configuration generator such as Ninja")
endif()
if(NOT MAIN_CACHE_CMAKE_BUILD_TYPE)
    message(FATAL_ERROR "main's CMake cache does not define CMAKE_BUILD_TYPE")
endif()
if(NOT CMAKE_BUILD_TYPE STREQUAL MAIN_CACHE_CMAKE_BUILD_TYPE)
    message(FATAL_ERROR
        "Analyzer build type '${CMAKE_BUILD_TYPE}' does not match main build type "
        "'${MAIN_CACHE_CMAKE_BUILD_TYPE}'")
endif()

get_filename_component(ANALYZER_COMPILER_REAL "${CMAKE_CXX_COMPILER}" REALPATH)
get_filename_component(MAIN_COMPILER_REAL "${MAIN_CACHE_CMAKE_CXX_COMPILER}" REALPATH)
if(NOT ANALYZER_COMPILER_REAL STREQUAL MAIN_COMPILER_REAL)
    message(FATAL_ERROR
        "Analyzer compiler '${CMAKE_CXX_COMPILER}' does not match main compiler "
        "'${MAIN_CACHE_CMAKE_CXX_COMPILER}'")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt5 5.15 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    OpenGL
    Concurrent
    Svg
    PrintSupport
    Xml)

if(NOT MAIN_CACHE_Qt5_DIR)
    message(FATAL_ERROR "main's CMake cache does not define Qt5_DIR")
endif()
get_filename_component(ANALYZER_QT5_DIR_REAL "${Qt5_DIR}" REALPATH)
get_filename_component(MAIN_QT5_DIR_REAL "${MAIN_CACHE_Qt5_DIR}" REALPATH)
if(NOT ANALYZER_QT5_DIR_REAL STREQUAL MAIN_QT5_DIR_REAL)
    message(FATAL_ERROR
        "Analyzer Qt package '${Qt5_DIR}' does not match main Qt package "
        "'${MAIN_CACHE_Qt5_DIR}'")
endif()

string(TOUPPER "${CMAKE_BUILD_TYPE}" ANALYZER_BUILD_TYPE_UPPER)
get_target_property(QT_CORE_LIBRARY Qt5::Core
    IMPORTED_LOCATION_${ANALYZER_BUILD_TYPE_UPPER})
if(NOT QT_CORE_LIBRARY)
    get_target_property(QT_CORE_LIBRARY Qt5::Core IMPORTED_LOCATION)
endif()
if(NOT QT_CORE_LIBRARY)
    message(FATAL_ERROR "Cannot determine the selected Qt 5 runtime library directory")
endif()
get_filename_component(QT_LIBRARY_DIR "${QT_CORE_LIBRARY}" DIRECTORY)

get_filename_component(MAIN_DEFAULT_LIBRARY_DIR
    "${MAIN_SOURCE_DIR}/../out_cmake/${MAIN_CACHE_CMAKE_BUILD_TYPE}/linux_x64/lib"
    ABSOLUTE)
set(MAIN_LIBRARY_DIR "${MAIN_DEFAULT_LIBRARY_DIR}"
    CACHE PATH "Directory containing main's built shared libraries")
get_filename_component(MAIN_LIBRARY_DIR "${MAIN_LIBRARY_DIR}" ABSOLUTE)
set(MAIN_QWT_LIBRARY "${MAIN_BUILD_DIR}/3rd_party/qwt/libqwt.a" CACHE FILEPATH
    "Qwt archive built as part of main")

if(NOT EXISTS "${MAIN_BUILD_DIR}/version.h")
    message(FATAL_ERROR
        "main's generated version.h is missing from ${MAIN_BUILD_DIR}; build main first")
endif()
if(NOT EXISTS "${MAIN_QWT_LIBRARY}")
    message(FATAL_ERROR
        "main's Qwt archive is missing: ${MAIN_QWT_LIBRARY}; build main first")
endif()

function(import_main_shared_library library_name)
    set(library_path "${MAIN_LIBRARY_DIR}/lib${library_name}.so")
    if(NOT EXISTS "${library_path}")
        message(FATAL_ERROR
            "main library '${library_name}' is missing: ${library_path}; build main first")
    endif()
    add_library(Main::${library_name} SHARED IMPORTED GLOBAL)
    set_target_properties(Main::${library_name} PROPERTIES
        IMPORTED_LOCATION "${library_path}")
endfunction()

foreach(main_library
        radardatabase
        radardata
        radarview
        radarmap
        xmlconfig)
    import_main_shared_library(${main_library})
endforeach()

add_library(Main::qwt STATIC IMPORTED GLOBAL)
set_target_properties(Main::qwt PROPERTIES
    IMPORTED_LOCATION "${MAIN_QWT_LIBRARY}")

set(ANALYZER_MAIN_INCLUDE_DIRS
    "${MAIN_SOURCE_DIR}/common_libs"
    "${MAIN_SOURCE_DIR}/common_libs/libradarmap"
    "${MAIN_SOURCE_DIR}/common_libs/libradardatabase"
    "${MAIN_SOURCE_DIR}/gui"
    "${MAIN_SOURCE_DIR}/3rd_party/qwt/src"
    "${MAIN_BUILD_DIR}")

add_executable(analyser
    main.cpp
    ampfiltertask.cpp
    acresolutiontask.cpp
    scenarioresolutiontask.cpp
    ampscatterdlg.cpp
    ampscattertask.cpp
    appwindow.cpp
    datasourcedlg.cpp
    datapack.cpp
    common.cpp
    duplicatetask.cpp
    falsepsrtask.cpp
    closepointstask.cpp
    guifilter.cpp
    podtask.cpp
    reflectorstask.cpp
    sampletask.cpp
    amplitudestask.cpp
    plotlabel.cpp
    amplitudedlg.cpp
    ampfiltertask.h
    acresolutiontask.h
    scenarioresolutiontask.h
    ampscatterdlg.h
    ampscattertask.h
    appwindow.h
    datasourcedlg.h
    datapack.h
    commondefs.h
    duplicatetask.h
    falsepsrtask.h
    closepointstask.h
    guifilter.h
    podtask.h
    ianalyser.h
    reflectorstask.h
    sampletask.h
    amplitudestask.h
    amplitudedlg.h
    plotlabel.h
    spline.h
    appwindow.ui
    ampscatter.ui
    datasource.ui
    amplitudedlg.ui)

target_include_directories(analyser PRIVATE ${ANALYZER_MAIN_INCLUDE_DIRS})
target_link_libraries(analyser PRIVATE
    Main::radardatabase
    Main::radardata
    Main::radarview
    Main::radarmap
    Main::xmlconfig
    Main::qwt
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    Qt5::OpenGL
    Qt5::Concurrent
    Qt5::Svg
    Qt5::PrintSupport
    Qt5::Xml)
set_target_properties(analyser PROPERTIES
    BUILD_RPATH "${MAIN_LIBRARY_DIR};${QT_LIBRARY_DIR}")

option(ANALYZER_BUILD_TOOLS "Build Analyzer command-line tools" ON)
if(ANALYZER_BUILD_TOOLS)
    add_executable(rdb_false_tracks_check
        tools/rdb_false_tracks_check/rdb_false_tracks_check.cpp)
    target_include_directories(rdb_false_tracks_check PRIVATE
        ${ANALYZER_MAIN_INCLUDE_DIRS})
    target_link_libraries(rdb_false_tracks_check PRIVATE
        Main::radardatabase
        Main::radardata
        Main::radarmap
        Main::xmlconfig
        Qt5::Core
        Qt5::Gui
        Qt5::Network)
    set_target_properties(rdb_false_tracks_check PROPERTIES
        BUILD_RPATH "${MAIN_LIBRARY_DIR};${QT_LIBRARY_DIR}")
endif()
