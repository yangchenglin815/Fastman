#-------------------------------------------------
#
# Project created by QtCreator 2016-03-21T11:40:51
#
#-------------------------------------------------

QT       += core gui xml

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = packer-configurator
DESTDIR = $$PWD/bin
TEMPLATE = app

win32:CONFIG(release, debug|release): {
    QMAKE_LFLAGS += /MANIFESTUAC:\"level=\'requireAdministrator\' uiAccess=\'false\'\"
}

SOURCES += main.cpp\
        mainwindow.cpp

HEADERS  += mainwindow.h

FORMS    += mainwindow.ui
