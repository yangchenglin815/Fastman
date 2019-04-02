TARGET = zmaster2

TEMPLATE = app
CONFIG -= qt
CONFIG += console

win32 {
DESTDIR = Z:/fastman2
}

unix {
DESTDIR = /media/ramdisk/fastman2
LIBS += -lz #-lpthread
QMAKE_POST_LINK += $$QMAKE_STRIP $$quote($$DESTDIR/$$TARGET)
}

DEFINES += CMD_DEBUG
DEFINES -= UNICODE
DEFINES += WIN32_LEAN_AND_MEAN
DEFINES += _CRT_SECURE_NO_WARNINGS
DEFINES += NO_QTLOG

INCLUDEPATH += "../"

HEADERS += \
    cprocess.h \
    ioapi.h \
    md5.h \
    msocket.h \
    qmutex.h \
    unzip.h \
    zapklsocketclient.h \
    zbytearray.h \
    zctools.h \
    zmaster2config.h \
    zmasterserver2.h \
    zmsg2.h \
    apkhelper.h \
    zlog.h

SOURCES += \
    cprocess.cpp \
    main.cpp \
    md5.cpp \
    qmutex.cpp \
    zapklsocketclient.cpp \
    zbytearray.cpp \
    zctools.cpp \
    zlog.cpp \
    zmasterserver2.cpp \
    zmsg2.cpp \
    ioapi.c \
    msocket.c \
    unzip.c \
    apkhelper.cpp \
    zlog.cpp
