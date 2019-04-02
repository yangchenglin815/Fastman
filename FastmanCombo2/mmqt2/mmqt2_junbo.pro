
QT  += core gui network webkit sql xml
greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets webkitwidgets
    DEFINES += USE_QT5
}

TARGET = mmqt2
TEMPLATE = app

DEFINES += USE_JUNBO

win32:CONFIG(release, debug|release): {
DESTDIR = Z:/fastman2
LIBS += -L"Z:/fastman2" -lzdm -llibcurl -lfastman2 -ladb -lqjson -lliblocalservice -lJunboInterface
DLL_SRC = $$PWD/qjson/qjson.dll
DLL_DEST = $$DESTDIR/qjson.dll
LIB_SRC = $$PWD/qjson/qjson.lib
LIB_DEST = $$DESTDIR/qjson.lib
CONFIG += embed_manifest_exe
QMAKE_LFLAGS += /MANIFESTUAC:\"level=\'requireAdministrator\' uiAccess=\'false\'\"
}

win32:CONFIG(debug, debug|release): {
DESTDIR = Z:/fastman2
LIBS += -L"Z:/fastman2" -lzdm -llibcurl_debug -lfastman2 -ladb -lqjsond -lliblocalservice -lJunboInterface
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

#CONFIG += console
#DEFINES += CMD_DEBUG
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
    formmain.cpp \
    main.cpp \
    bundle.cpp \
    formdownload.cpp \
    zlog.cpp \
    zthreadpool.cpp \
    globalcontroller.cpp \
    formdevicenode.cpp \
    formbundlenode.cpp \
    formweb.cpp \
    zwebview.cpp \
    loginmanager.cpp \
    appconfig.cpp \
    dialogwait.cpp \
    zadbdevicenode.cpp \
    zadbdevicejobthread.cpp \
    dialogadddownload.cpp \
    forminstallation.cpp \
    dialogcmd.cpp \
    popup.cpp \
    dialoglogin.cpp \
    dialogbundleview.cpp \
    dialogdeviceview.cpp \
    formbundlelist.cpp \
    zstringutil.cpp \
    dialogprogress.cpp \
    dialogsetup.cpp \
    dialogtidyapk.cpp \
    formdevicelog.cpp \
    formwebapps.cpp \
    dialogfeedback.cpp \
    dialogseltoclean.cpp\
    formwebindex.cpp \
    dialogdelwhitelist.cpp \
    dialoghint.cpp \
    dialoglogview.cpp \
    dialoglogshow.cpp \
    dialogrootflags.cpp \
    dialogjunbologin.cpp
HEADERS  += \ 
    formmain.h \
    bundle.h \
    formdownload.h \
    zthreadpool.h \
    globalcontroller.h \
    formdevicenode.h \
    formbundlenode.h \
    formweb.h \
    zwebview.h \
    loginmanager.h \
    appconfig.h \
    dialogwait.h \
    zadbdevicenode.h \
    zadbdevicejobthread.h \
    dialogadddownload.h \
    forminstallation.h \
    dialogcmd.h \
    popup.h \
    dialoglogin.h \
    dialogbundleview.h \
    dialogdeviceview.h \
    formbundlelist.h \
    zstringutil.h \
    dialogprogress.h \
    dialogsetup.h \
    dialogtidyapk.h \
    formdevicelog.h \
    formwebapps.h \
    dialogfeedback.h \
    dialogseltoclean.h\
    formwebindex.h \
    dialogdelwhitelist.h \
    dialoghint.h \
    dialoglogview.h \
    dialoglogshow.h \
    dialogrootflags.h \
    junbointerface.h \
    dialogjunbologin.h
FORMS += \ 
    formmain.ui \
    formdownload.ui \
    formdevicenode.ui \
    formbundlenode.ui \
    formweb.ui \
    dialogadddownload.ui \
    forminstallation.ui \
    dialogcmd.ui \
    dialoglogin.ui \
    dialogbundleview.ui \
    dialogdeviceview.ui \
    formbundlelist.ui \
    dialogprogress.ui \
    dialogsetup.ui \
    dialogtidyapk.ui \
    formdevicelog.ui \
    formwebapps.ui \
    dialogfeedback.ui \
    dialogseltoclean.ui\
    formwebindex.ui \
    dialogdelwhitelist.ui \
    dialoghint.ui \
    dialoglogview.ui \
    dialoglogshow.ui \
    dialogrootflags.ui \
    dialogjunbologin.ui

RESOURCES += \
    skins.qrc \
    ui.qrc

RC_FILE = mmqt2.rc
