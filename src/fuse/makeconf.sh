#! /bin/sh

echo Running libtoolize...
libtoolize --automake -c -f

if test ! -z "`which autoreconf`"; then
    echo Running autoreconf...
    autoreconf -i -f
else
    echo Running aclocal...
    aclocal
    echo Running autoheader...
    autoheader
    echo Running autoconf...
    autoconf
    echo Running automake...
    automake -a -c
    (
	echo Entering directory: kernel
	cd kernel
	echo Running autoheader...
	autoheader
	echo Running autoconf...
	autoconf
    )
fi
echo Linking kernel header file...
ln -sf ../kernel/fuse_kernel.h `dirname $0`/include

rm -f config.cache config.status
echo "To compile run './configure', and then 'make'."
