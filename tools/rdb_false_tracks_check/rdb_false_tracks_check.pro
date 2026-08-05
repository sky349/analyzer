QT += core gui network

CONFIG += console c++11
CONFIG -= app_bundle

TEMPLATE = app
TARGET = rdb_false_tracks_check

SOURCES += rdb_false_tracks_check.cpp

INCLUDEPATH += ../../../../../out/include

LIBS += -L$$PWD/../../../../../out/lib \
    -lradardatabase \
    -lradardata \
    -lradarmap \
    -lxmlconfig

win32:DEFINES += WIN32
