#-------------------------------------------------
#
# Project created by QtCreator 2014-07-23T14:04:43
#
#-------------------------------------------------

QT       += core gui network
#QT -= gui
#DEFINES += NO_GUI

TARGET = localserviceTest
TEMPLATE = app

win32 {
DESTDIR = Z:/fastman2
CONFIG += embed_manifest_exe
QMAKE_LFLAGS += /MANIFESTUAC:\"level=\'requireAdministrator\' uiAccess=\'false\'\"
}

unix {
DESTDIR = /media/ramdisk/fastman2
}

HEADER_SRC = $$PWD/*.h
HEADER_DEST = $$DESTDIR/
win32 {
HEADER_SRC ~= s,/,\\,g
HEADER_DEST ~= s,/,\\,g
}
QMAKE_POST_LINK += $$QMAKE_COPY $$quote($$HEADER_SRC) $$quote($$HEADER_DEST)

CONFIG += console
DEFINES += CMD_DEBUG
DEFINES -= UNICODE
DEFINES += WIN32_LEAN_AND_MEAN
DEFINES += _CRT_SECURE_NO_WARNINGS

INCLUDEPATH += "$$DESTDIR/"

SOURCES += \
    zlocalserver.cpp \
    zlocalclient.cpp \
    zsingleprocesshelper.cpp \
    test.cpp

HEADERS += \
    zlocalserver.h \
    zlocalclient.h \
    zlog.h \
    zsingleprocesshelper.h
