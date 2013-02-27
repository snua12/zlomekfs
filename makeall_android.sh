#!/bin/sh

build_libconfig()
{
	mkdir -p libconfig && cd libconfig
	wget http://www.hyperrealm.com/libconfig/libconfig-1.4.9.tar.gz
	tar xzf libconfig-1.4.9.tar.gz
	cd libconfig-1.4.9

	NDK_CTOOL_PATH="${ANDROID_NDK}/toolchains/arm-linux-androideabi-4.4.3/prebuilt/linux-x86/bin/"
	export SYS_ROOT="${ANDROID_NDK}/platforms/android-14/arch-arm/"
	export CC="${NDK_CTOOL_PATH}arm-linux-androideabi-gcc --sysroot=$SYS_ROOT"
	export CXX="${NDK_CTOOL_PATH}arm-linux-androideabi-g++ --sysroot=$SYS_ROOT"
	export LD="${NDK_CTOOL_PATH}arm-linux-androideabi-ld"
	export AR="${NDK_CTOOL_PATH}arm-linux-androideabi-ar"
	export RANLIB="${NDK_CTOOL_PATH}arm-linux-androideabi-ranlib"
	export STRIP="${NDK_CTOOL_PATH}arm-linux-androideabi-strip"

	./configure --host=arm-eabi --disable-cxx --disable-examples LIBS="-lc -lgcc"

	make

	export LCONFIG_LIBRARY_DIRS=$(pwd)/lib/.libs/
	export LCONFIG_INCLUDE_DIRS=$(pwd)/lib/

	cd ../../
}

build_fuse()
{
	mkdir -p libfuse && cd libfuse
	git clone https://github.com/seth-hg/fuse-android.git
	cd fuse-android

	export FUSE_LIBRARY_DIRS=$(pwd)/obj/local/armeabi/
	export FUSE_INCLUDE_DIRS=$(pwd)/jni/include/

	"${ANDROID_NDK}/ndk-build"

	cd ../../
}

build_thirdparty()
{
	mkdir -p third_party && cd third_party

	build_libconfig

	build_fuse

	cd ..
}

if [ -z "${ANDROID_NDK}" ]; then
	echo "Please set ANDROID_NDK"
	echo "  export ANDROID_NDK=\"/absolute/path/to/the/android-ndk\""
	exit 1
fi

mkdir -p build_android && cd build_android

build_thirdparty

cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/android.toolchain.cmake \
	-DFUSE_LIBRARY_DIRS="${FUSE_LIBRARY_DIRS}" \
	-DFUSE_INCLUDE_DIRS="${FUSE_INCLUDE_DIRS}" \
	-DLCONFIG_LIBRARY_DIRS="${LCONFIG_LIBRARY_DIRS}" \
	-DLCONFIG_INCLUDE_DIRS="${LCONFIG_INCLUDE_DIRS}" \
	.. 

make 

