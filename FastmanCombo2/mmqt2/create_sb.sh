#!/bin/sh

if [ ! -f /usr/share/slax/slaxbuildlib ]
then
  echo "only works in slax linux"
  exit 1
fi

if [ $# -ne 2 ]
then
  echo
  echo "create_sb.sh <out_name> <appname>"
  echo
  exit 1
fi

OUT_NAME="$1"
APP_NAME="$2"

echo "outname=${OUT_NAME}, appname=${APP_NAME}"

cat << EOF > "${OUT_NAME}.desktop"
[Desktop Entry]
Comment[zh_CN]=
Comment=
Exec=/mnt/live/memory/data/18slax/fastman2/start.sh
GenericName[zh_CN]=
GenericName=
Icon=/mnt/live/memory/data/18slax/fastman2/icon.png
MimeType=
Name[zh_CN]=${APP_NAME}
Name=${APP_NAME}
Path=
StartupNotify=true
Terminal=false
TerminalOptions=
Type=Application
X-DBUS-ServiceName=
X-DBUS-StartupType=
X-KDE-SubstituteUID=false
X-KDE-Username=
Categories=Network;
EOF

. /usr/share/slax/slaxbuildlib
SLAX_BUNDLE_NAME="${OUT_NAME}"
SLAX_BUNDLE_VERSION="1.0"

SLAX_BUNDLE_DESCRIPTION="${OUT_NAME}"
SLAX_BUNDLE_CATEGORIES="network"
SLAX_BUNDLES_REQUIRED=""
SLAX_BUNDLES_REQUIRED_TO_COMPILE_ONLY=""

SLAX_BUNDLE_MAINTAINER_NAME="fay"
SLAX_BUNDLE_MAINTAINER_EMAIL="fay@dengxian.net"

SLAX_BUNDLE_SOURCE_DOWNLOAD[0]="fake"

check_variables_for_errors
init_bundle_target_dir

cd "${SLAX_CURRENT_BUILDSCRIPT_DIR}"

mkdir -p "${SLAX_BUNDLE_TARGET}/root/桌面/"
mkdir -p "${SLAX_BUNDLE_TARGET}/usr/share/applications/"

cp "${OUT_NAME}.desktop" "${SLAX_BUNDLE_TARGET}/root/桌面/"
cp "${OUT_NAME}.desktop" "${SLAX_BUNDLE_TARGET}/usr/share/applications/"

create_slax_bundle
