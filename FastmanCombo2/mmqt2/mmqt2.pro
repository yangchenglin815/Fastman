
QT  += core gui network webkit sql xml script
greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets webkitwidgets
    DEFINES += USE_QT5
}

TARGET = mmqt2
TEMPLATE = app

include(../defines.pri)

win32:CONFIG(release, debug|release): {
LIBS += -L"$$DESTDIR" -lzdm -llibcurl -lfastman2 -ladb
CONFIG += embed_manifest_exe
QMAKE_LFLAGS += /MANIFESTUAC:\"level=\'requireAdministrator\' uiAccess=\'false\'\"
}

win32:CONFIG(debug, debug|release): {
LIBS += -L"$$DESTDIR" -lzdm -llibcurl_debug -lfastman2 -ladb
}

win32 {
LIBS += -lsetupapi
DEFINES += USE_DRIVERS
}

macx {
LIBS += -L"$$DESTDIR" -lzdm -lcurl -lfastman2 -ladb -lz -lpthread
}

unix:!macx {
LIBS += -L"$$DESTDIR" -lzdm -lcurl -lrt -lfastman2 -ladb -lz -lpthread
}

unix {
QMAKE_POST_LINK += $$QMAKE_STRIP $$quote($$DESTDIR/$$TARGET)
}

INCLUDEPATH += "../_Public_util_sources"
INCLUDEPATH += "../LibLocalService"
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
    ../_Public_util_sources/QJson/serializerrunnable.cpp \
    ../_Public_util_sources/QJson/serializer.cpp \
    ../_Public_util_sources/QJson/qobjecthelper.cpp \
    ../_Public_util_sources/QJson/parserrunnable.cpp \
    ../_Public_util_sources/QJson/parser.cpp \
    ../_Public_util_sources/QJson/json_scanner.cpp \
    ../_Public_util_sources/QJson/json_scanner.cc \
    ../_Public_util_sources/QJson/json_parser.cc \
    ../LibLocalService/zlocalclient.cpp \
    ../LibLocalService/zlocalserver.cpp \
    ../LibLocalService/zsingleprocesshelper.cpp \
    ../_Public_util_sources/zsettings.cpp \
    ../_Public_util_sources/ziphelper.cpp \
    ztimer.cpp \
    zapplication.cpp \
    zLaucherManager.cpp \
    launchermaker.cpp \
    launcherconfig.cpp \
    launchereditor.cpp \
    dialogconfigwifi.cpp \
    dialogdesktopini.cpp \
    ../_Public_util_sources/zlog.cpp \
    dialogconfiglan.cpp


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
    ../_Public_util_sources/QJson/serializerrunnable.h \
    ../_Public_util_sources/QJson/parserrunnable.h \
    ../LibLocalService/zlocalclient.h \
    ../LibLocalService/zlocalserver.h \
    ../LibLocalService/zsingleprocesshelper.h \
    ../_Public_util_sources/zsettings.h \
    ../_Public_util_sources/ziphelper.h \
    ztimer.h \
    zapplication.h \
    zLaucherManager.h \
    launchereditor.h \
    launchermaker.h \
    launcherconfig.h \
    dialogconfigwifi.h \
    dialogdesktopini.h \
    ../_Public_util_sources/zlog.h \
    dialogconfiglan.h

win32 {
   SOURCES += ../_Public_util_sources/winverifytrust.cpp
   HEADERS  += ../_Public_util_sources/winverifytrust.h
}

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
    dialogconfigwifi.ui \
    dialogdesktopini.ui \
    dialogconfiglan.ui

RESOURCES += \
    skins.qrc \
    ui.qrc

contains(DEFINES, USE_DRIVERS) {
SOURCES += \
    ../_Public_util_sources/driverhelper.cpp \
    formdriverhint.cpp

HEADERS  += \
    ../_Public_util_sources/driverhelper.h \
    formdriverhint.h

FORMS += \
    formdriverhint.ui
}

RC_FILE = mmqt2.rc
