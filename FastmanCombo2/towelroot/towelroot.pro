TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

TARGET = tr

DESTDIR = /media/ramdisk/fastman2
QMAKE_POST_LINK += $$QMAKE_STRIP $$quote($$DESTDIR/$$TARGET)

SOURCES += main.cpp

HEADERS += \
    tr2.h
