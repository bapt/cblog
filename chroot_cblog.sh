#!/bin/sh

# Helper for chroot cblog

if [ $(id -u) -ne 0 ]; then
    echo "This script must be run as root"
    exit 1
fi

WORKDIR="./chroot_cblog"

DIRS="
    dev
    tmp
    var/run
    var/db
    lib
    usr/local/etc
    usr/local/libexec/cgi-bin
    usr/local/share/cblog/templates
    libexec
"

mkdir $WORKDIR
cd $WORKDIR

mkdir -p $DIRS

cp /lib/libc.so.7 lib/
cp /lib/libz.so.5 lib/
cp /libexec/ld-elf.so.1 libexec/
cp /usr/local/lib/libfcgi.so.0 lib/
cp ../samples/cblog.conf usr/local/etc/
cp ../samples/templates/* usr/local/share/cblog/templates/
cp ../cgi/cblog.cgi usr/local/libexec/cgi-bin/

tar czvf ../chroot_cblog.tgz .
cd ..
rm -rf $WORKDIR
