#-------------------------------------------------
#
# Project created by QtCreator 2014-06-04T13:38:30
#
#-------------------------------------------------

QT       += core gui network

TARGET = mmqt2_helper
TEMPLATE = app

include(../defines.pri)
DEFINES -= CMD_DEBUG

win32 {
CONFIG += embed_manifest_exe
QMAKE_LFLAGS += /MANIFESTUAC:\"level=\'requireAdministrator\' uiAccess=\'false\'\"
}

LIBS += -L"$$DESTDIR" -ladb

INCLUDEPATH += "../LibLocalService"
INCLUDEPATH += "../"
INCLUDEPATH += "../adb"
INCLUDEPATH += "../fastman2"

SOURCES += main.cpp\
    trackermain.cpp \
    cadbdevicenode.cpp \
    ../LibLocalService/zlocalclient.cpp \
    ../LibLocalService/zlocalserver.cpp \
    ../LibLocalService/zsingleprocesshelper.cpp

HEADERS  += trackermain.h \
    cadbdevicenode.h \
    ../LibLocalService/zlocalclient.h \
    ../LibLocalService/zlocalserver.h \
    ../LibLocalService/zsingleprocesshelper.h
