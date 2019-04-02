TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

TARGET = vr

DESTDIR = /media/ramdisk/fastman2
QMAKE_POST_LINK += $$QMAKE_STRIP $$quote($$DESTDIR/$$TARGET)

SOURCES += main.cpp \
    vroot.cpp \
    base64.cpp \
    md5.cpp \
    cprocess.cpp \
    zbytearray.cpp

HEADERS += \
    base64.h \
    md5.h \
    cprocess.h \
    zbytearray.h
