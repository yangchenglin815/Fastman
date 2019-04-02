#!/bin/sh

if [ $# -eq 0 ]
then
    BASEPATH="${0%/*}"
else
    BASEPATH="$1"
fi

if [ ! -e "${BASEPATH}/mmqt2" ]
then
    kdialog --msgbox "可执行文件不存在，请进入WINDOWS系统重新安装"
    exit 1
fi

if [ ! -e /usr/lib/libqjson.so.0 ]
then
    cp "${BASEPATH}/libqjson.so" /usr/lib/libqjson.so.0
    chmod 755 /usr/lib/libqjson.so.0
fi

if [ ! -e /usr/bin/zxlycon ]
then
    cp "${BASEPATH}/zxlycon" /usr/bin/zxlycon
    chmod 755 /usr/bin/zxlycon
fi

if [ ! -e /usr/bin/aapt ]
then
    cp "${BASEPATH}/aapt" /usr/bin/aapt
    chmod 755 /usr/bin/aapt
fi

"${BASEPATH}/mmqt2"
