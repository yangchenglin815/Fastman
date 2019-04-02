
QT  += core network gui
#QT -= gui
#DEFINES += NO_GUI

TARGET = adb
TEMPLATE = lib
CONFIG += staticlib

include(../defines.pri)
DEFINES -= CMD_DEBUG

INCLUDEPATH += "../"
INCLUDEPATH += "../_Public_util_sources"

HEADER_SRC = $$PWD/*.h
HEADER_DEST = $$DESTDIR/
win32 {
HEADER_SRC ~= s,/,\\,g
HEADER_DEST ~= s,/,\\,g
}
QMAKE_POST_LINK += $$QMAKE_COPY $$quote($$HEADER_SRC) $$quote($$HEADER_DEST)

SOURCES += \ 
    adbclient.cpp \
    adbdevicejobthread.cpp \
    adbdevicenode.cpp \
    adbtracker.cpp \
    adbprocess.cpp
HEADERS  += \ 
    adbclient.h \
    adbdevicejobthread.h \
    adbdevicenode.h \
    adbtracker.h \
    adbprocess.h
