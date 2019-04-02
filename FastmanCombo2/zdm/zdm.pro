
QT  += core network xml
#QT -= gui
#DEFINES += NO_GUI

TARGET = zdm
TEMPLATE = lib
CONFIG += staticlib

include(../defines.pri)
DEFINES -= CMD_DEBUG

INCLUDEPATH += "../"
INCLUDEPATH += "../_Public_util_sources"

win32:CONFIG(release, debug|release): {
INCLUDEPATH += "$$PWD/include"
LIB_SRC = $$PWD/lib/libcurl.lib
LIB_DEST = $$DESTDIR/libcurl.lib
}

win32:CONFIG(debug, debug|release): {
INCLUDEPATH += "$$PWD/include"
LIB_SRC = $$PWD/lib/libcurl_debug.lib
LIB_DEST = $$DESTDIR/libcurl_debug.lib
}

macx {
INCLUDEPATH += "$$PWD/include_mac"
LIB_SRC = $$PWD/lib/libcurl_mac.a
LIB_DEST = $$DESTDIR/libcurl.a
}

unix:!macx {
INCLUDEPATH += "$$PWD/include_linux"
LIB_SRC = $$PWD/lib/libcurl.a
LIB_DEST = $$DESTDIR/libcurl.a
}

HEADER_SRC = $$PWD/*.h
HEADER_DEST = $$DESTDIR/

win32 {
HEADER_SRC ~= s,/,\\,g
HEADER_DEST ~= s,/,\\,g
LIB_SRC ~= s,/,\\,g
LIB_DEST ~= s,/,\\,g
}
QMAKE_POST_LINK += $$QMAKE_COPY $$quote($$HEADER_SRC) $$quote($$HEADER_DEST)
QMAKE_POST_LINK += && $$QMAKE_COPY $$quote($$LIB_SRC) $$quote($$LIB_DEST)

SOURCES += zdmprivate.cpp \
    zdmhttp.cpp \
    zdmhttpex.cpp

HEADERS  += zdownloadmanager.h \
    zdmprivate.h \
    zdmhttp.h \
    zdmhttpex.h
