#-------------------------------------------------
#
# Project created by QtCreator 2013-06-24T16:12:32
#
#-------------------------------------------------

QT       += core gui widgets network opengl
CONFIG += debug_and_release #console

TARGET = analyser
TEMPLATE = app

SOURCES += main.cpp\
    ampfiltertask.cpp \
    ampscatterdlg.cpp \
    ampscattertask.cpp \
		appwindow.cpp \
	datasourcedlg.cpp \
	datapack.cpp \
	common.cpp \
    duplicatetask.cpp \
    falsepsrtask.cpp \
	guifilter.cpp \
	podtask.cpp \
	reflectorstask.cpp \
	sampletask.cpp \
	amplitudestask.cpp \
	amplitudedlg.cpp

HEADERS  += appwindow.h \
	ampfiltertask.h \
	ampscatterdlg.h \
	ampscattertask.h \
	datasourcedlg.h \
	datapack.h \
	commondefs.h \
	duplicatetask.h \
	falsepsrtask.h \
	guifilter.h \
	podtask.h \
	ianalyser.h \
	reflectorstask.h \
	sampletask.h \
	amplitudestask.h \
	amplitudedlg.h \
	spline.h

INCLUDEPATH += ../../../out/include \
                ../../../src/3rd_party/qwt/include

LIBS += -L../../../out/lib \
	-lradardatabase \
	-lradardata \
	-lradarview \
	-lradarmap \
        -lxmlconfig \
        -lqwt

FORMS    += appwindow.ui \
	ampscatter.ui \
	datasource.ui \
	amplitudedlg.ui

QMAKE_CXXFLAGS += -std=c++0x
win32:DEFINES += WIN32

CONFIG+=exceptions rtti # defaults changed in 4.8.3
