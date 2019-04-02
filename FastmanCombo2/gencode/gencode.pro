TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

win32 {
DESTDIR = E:/FastmanCombo2/Trunk/fastman2
}

unix {
DESTDIR = /media/ramdisk/fastman2
}

QMAKE_POST_LINK += $$DESTDIR/gencode $$PWD vroot267 vroot253 vroot325 vroot_i9308 vroot_p7 vroot_s4 vroot_small vroot_uniscope vroot265 vroot477 vroot171 vroot292 vroot_su -o ../vroot/binaries.h
QMAKE_POST_LINK += && $$DESTDIR/gencode $$PWD pm debuggerd -o ../zmaster2/protect_bin.h
QMAKE_POST_LINK += && $$DESTDIR/gencode $$PWD \
    su su_mips su_pie su_x86 \
    zmaster2 zmaster2_mips zmaster2_pie zmaster2_x86 \
    tr vr \
    hacked_AndroidManifest.xml blank.apk frama.apk \
    -o ../fastman2/binaries.h

DEFINES -= UNICODE
DEFINES += WIN32_LEAN_AND_MEAN
DEFINES += _CRT_SECURE_NO_WARNINGS

SOURCES += \
    gencode.c

HEADERS += \
    gencode.h

