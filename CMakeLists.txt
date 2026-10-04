cmake_minimum_required(VERSION 3.12)

project(analyzer LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Network OpenGL Svg)

set(MAIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../main")
set(MAIN_BUILD_DIR "${MAIN_DIR}/../build/Desktop_Qt_5_15_2_GCC_64bit-Release")
set(MAIN_LIBRARY_DIR "${MAIN_DIR}/../out_cmake/Release/linux_x64/lib")

file(GLOB ANALYZER_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(analyser ${ANALYZER_SOURCES} ianalyser.h)

target_include_directories(analyser PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${MAIN_DIR}/common_libs"
    "${MAIN_DIR}/gui"
    "${MAIN_DIR}/3rd_party/qwt/src"
    "${MAIN_BUILD_DIR}"
    # Header-only production code reused verbatim, e.g. <processor/rdps3/dopplerrestorer.h>.
    "${MAIN_DIR}")

target_link_libraries(analyser PRIVATE
    "${MAIN_LIBRARY_DIR}/libradardatabase.so"
    "${MAIN_LIBRARY_DIR}/libradardata.so"
    "${MAIN_LIBRARY_DIR}/libradarview.so"
    "${MAIN_LIBRARY_DIR}/libradarmap.so"
    "${MAIN_BUILD_DIR}/3rd_party/qwt/libqwt.a"
    Qt5::Widgets
    Qt5::Network
    Qt5::OpenGL
    Qt5::Svg)

# Explicit target: archive/legacy regressions do not become part of an ordinary application build.
add_executable(prp3recordingtests EXCLUDE_FROM_ALL
    tests/prp3recordingtests.cpp
    prp3logreader.cpp
    prp3signalwindow.cpp
    prp3trackerevidence.cpp
    trackerevidencepanel.cpp
    "${MAIN_DIR}/processor/rdps3/prp3plotlog.cpp"
    "${MAIN_DIR}/processor/rdps3/prparchive.cpp"
    "${MAIN_DIR}/processor/rdps3/prefilterssr.cpp"
    "${MAIN_DIR}/processor/rdps3/logger.cpp"
    "${MAIN_DIR}/processor/rdps3/tentativetrackevidence.cpp")
target_include_directories(prp3recordingtests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}" "${MAIN_DIR}" "${MAIN_BUILD_DIR}"
    "${MAIN_DIR}/common_libs" "${MAIN_DIR}/processor/rdps3" "${MAIN_DIR}/3rd_party/qwt/src")
target_link_libraries(prp3recordingtests PRIVATE
    "${MAIN_LIBRARY_DIR}/libsiteconfig.so"
    "${MAIN_LIBRARY_DIR}/libradardata.so"
    "${MAIN_LIBRARY_DIR}/libradarmap.so"
    "${MAIN_BUILD_DIR}/3rd_party/qwt/libqwt.a"
    Qt5::Widgets Qt5::Network Qt5::Svg)
