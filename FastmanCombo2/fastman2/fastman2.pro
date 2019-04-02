
QT  += core gui network
#QT -= gui
#DEFINES += NO_GUI

TARGET = fastman2
TEMPLATE = lib
CONFIG += staticlib

include(../defines.pri)
DEFINES -= CMD_DEBUG

INCLUDEPATH += "../"
INCLUDEPATH += "../adb"
INCLUDEPATH += $$PWD/../_Public_util_sources

HEADER_SRC = $$PWD/fastman2.h
HEADER_DEST = $$DESTDIR/
win32 {
HEADER_SRC ~= s,/,\\,g
HEADER_DEST ~= s,/,\\,g
}
QMAKE_POST_LINK += $$QMAKE_COPY $$quote($$HEADER_SRC) $$quote($$HEADER_DEST)

SOURCES += \ 
    ziphacker.cpp \
    zbytearray.cpp \
    jdwpwalker.cpp \
    zip.c \
    unzip.c \
    ioapi.c \
    zmsg2.cpp \
    fastmansocket.cpp \
    fastman2.cpp

HEADERS  += \ 
    ziphacker.h \
    zip.h \
    zbytearray.h \
    unzip.h \
    jdwpwalker.h \
    ioapi.h \
    zmsg2.h \
    fastmansocket.h \
    zdatabaseutil.h \
    fastman2.h \    
    resource.h \
    binaries.h

FORMS += 
