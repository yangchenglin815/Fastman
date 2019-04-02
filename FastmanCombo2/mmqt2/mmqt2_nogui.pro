
QT  += core network sql xml
greaterThan(QT_MAJOR_VERSION, 4) {
    DEFINES += USE_QT5
}

QT -= gui
DEFINES += NO_GUI

TARGET = mmqt2_cli
TEMPLATE = app

win32:CONFIG(release, debug|release): {
DESTDIR = Z:/fastman2
LIBS += -L"Z:/fastman2" -lzdm -llibcurl -lfastman2 -ladb -lqjson -lliblocalservice
DLL_SRC = $$PWD/qjson/qjson.dll
DLL_DEST = $$DESTDIR/qjson.dll
LIB_SRC = $$PWD/qjson/qjson.lib
LIB_DEST = $$DESTDIR/qjson.lib
#CONFIG += embed_manifest_exe
#QMAKE_LFLAGS += /MANIFESTUAC:\"level=\'requireAdministrator\' uiAccess=\'false\'\"
}

win32:CONFIG(debug, debug|release): {
DESTDIR = Z:/fastman2
LIBS += -L"Z:/fastman2" -lzdm -llibcurl_debug -lfastman2 -ladb -lqjsond -lliblocalservice
DLL_SRC = $$PWD/qjson/qjsond.dll
DLL_DEST = $$DESTDIR/qjsond.dll
LIB_SRC = $$PWD/qjson/qjsond.lib
LIB_DEST = $$DESTDIR/qjsond.lib
}

unix {
DESTDIR = /media/ramdisk/fastman2
LIBS += -L"/media/ramdisk/fastman2" -lzdm -lcurl -lrt -lfastman2 -ladb -lqjson -lz -lpthread -lliblocalservice
LIB_SRC = $$PWD/qjson/libqjson.so
LIB_DEST = $$DESTDIR/libqjson.so
}

win32 {
DLL_SRC ~= s,/,\\,g
DLL_DEST ~= s,/,\\,g
LIB_SRC ~= s,/,\\,g
LIB_DEST ~= s,/,\\,g
}

QMAKE_PRE_LINK += $$QMAKE_COPY $$quote($$LIB_SRC) $$quote($$LIB_DEST)
win32 {
QMAKE_POST_LINK += $$QMAKE_COPY $$quote($$DLL_SRC) $$quote($$DLL_DEST)
}
unix {
QMAKE_POST_LINK += $$QMAKE_STRIP $$quote($$DESTDIR/$$TARGET)
}

CONFIG += console
DEFINES += CMD_DEBUG
DEFINES += NEW_INTERFACE

DEFINES -= UNICODE
DEFINES += _CRT_SECURE_NO_WARNINGS
DEFINES += WIN32_LEAN_AND_MEAN

INCLUDEPATH += "$$PWD/qjson"
INCLUDEPATH += "../"
INCLUDEPATH += "../adb"
INCLUDEPATH += "../fastman2"
INCLUDEPATH += "../zdm"
INCLUDEPATH += "$$DESTDIR/"

SOURCES += \ 
    main.cpp \
    bundle.cpp \
    zlog.cpp \
    zthreadpool.cpp \
    globalcontroller.cpp \
    loginmanager.cpp \
    appconfig.cpp \
    zadbdevicenode.cpp \
    zadbdevicejobthread.cpp \
    zstringutil.cpp \
    coremain.cpp \
    dialoglogin.cpp
HEADERS  += \ 
    bundle.h \
    zthreadpool.h \
    globalcontroller.h \
    loginmanager.h \
    appconfig.h \
    zadbdevicenode.h \
    zadbdevicejobthread.h \
    zstringutil.h \
    coremain.h \
    dialoglogin.h

RC_FILE = mmqt2.rc
