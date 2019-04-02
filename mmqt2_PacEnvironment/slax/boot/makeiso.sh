#!/bin/sh
# ---------------------------------------------------
# Script to create bootable Slax ISO in Linux
# author: Tomas M. <http://www.slax.org>
# ---------------------------------------------------

if [ "$1" = "--help" -o "$1" = "-h" -o "$2" = "" ]; then
  echo "This script will create bootable ISO" >&2
  echo "usage:   $0 [ source directory tree ] [ destination.iso ]" >&2
  echo "example: $0 /mnt/sda1/my-slax-cd-tree/ /mnt/hda5/new-slax.iso" >&2
  exit 1
fi

MKISOFS=$(which mkisofs)

if [ "$MKISOFS" = "" ]; then
   echo "mkisofs not found. Please install the appropriate package (eg. cdrtools)" >&2
   exit 3
fi

TARGET="$(readlink "$2" || echo "$2")"

cd "$1"

if [ -r boot/isolinux.bin ]; then
   echo "Folder $1 contains 'boot', going one directory up to find 'slax'."
   cd ..
fi

if [ ! -r slax/boot/isolinux.bin ]; then
   echo "Directory $(pwd) is not valid Slax directory."
   exit 2
fi

mkisofs -o "$TARGET" -v -J -R -D -A "Slax" -V "Slax CD/DVD" \
-no-emul-boot -boot-info-table -boot-load-size 4 \
-b slax/boot/isolinux.bin -c slax/boot/isolinux.boot .
