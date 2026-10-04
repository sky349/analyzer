cmake_minimum_required(VERSION 3.16)
project(analyzer_recording_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets)

get_filename_component(MAIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../main" ABSOLUTE)
add_executable(prp3recordingtests
    prp3recordingtests.cpp
    ../prp3logreader.cpp
    ../prp3trackerevidence.cpp
    ../trackerevidencepanel.cpp
    "${MAIN_DIR}/processor/rdps3/prp3plotlog.cpp"
    "${MAIN_DIR}/processor/rdps3/prparchive.cpp"
    "${MAIN_DIR}/processor/rdps3/tentativetrackevidence.cpp")
target_include_directories(prp3recordingtests PRIVATE .. "${MAIN_DIR}" "${MAIN_DIR}/common_libs")
target_link_libraries(prp3recordingtests PRIVATE Qt5::Widgets)
enable_testing()
add_test(NAME prp3recordingtests COMMAND prp3recordingtests)
set_tests_properties(prp3recordingtests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
