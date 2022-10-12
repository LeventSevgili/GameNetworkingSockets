#!/bin/bash

echo "$(tput setaf 2)"
echo "###################################################################"
echo "# Preparing to build Google Protobuf"
echo "###################################################################"
echo "$(tput sgr0)"

# The results will be stored relative to the location
# where you stored this script, **not** relative to
# the location of the protobuf git repo.
PREFIX=`pwd`/protobuf
if [ -d ${PREFIX} ]
then
    rm -rf "${PREFIX}"
fi
mkdir -p "${PREFIX}/platform"

# A "YES" value will build the latest code from GitHub on the master branch.
# A "NO" value will use the 2.6.0 tarball downloaded from googlecode.com.
USE_GIT_MASTER=NO

PROTOBUF_GIT_URL=https://github.com/google/protobuf.git
PROTOBUF_GIT_DIRNAME=protobuf
PROTOBUF_VERSION=3.21.7
PROTOBUF_RELEASE_URL=https://github.com/protocolbuffers/protobuf/archive/refs/tags/v21.7.zip
PROTOBUF_RELEASE_DIRNAME=protobuf-${PROTOBUF_VERSION}

BUILD_MACOSX_X86_64=NO

BUILD_I386_IOSSIM=NO
BUILD_X86_64_IOSSIM=NO

BUILD_IOS_ARMV7=NO
BUILD_IOS_ARMV7S=NO
BUILD_IOS_ARM64=YES

PROTOBUF_SRC_DIR=./

DARWIN=darwin`uname -r`

XCODEDIR=`xcode-select --print-path`
IOS_SDK_VERSION=`xcrun --sdk iphoneos --show-sdk-version`
MIN_SDK_VERSION=11.0

MACOSX_PLATFORM=${XCODEDIR}/Platforms/MacOSX.platform
MACOSX_SYSROOT=${MACOSX_PLATFORM}/Developer/SDKs/MacOSX10.10.sdk

IPHONEOS_PLATFORM=`xcrun --sdk iphoneos --show-sdk-platform-path`
IPHONEOS_SYSROOT=`xcrun --sdk iphoneos --show-sdk-path`

IPHONESIMULATOR_PLATFORM=`xcrun --sdk iphonesimulator --show-sdk-platform-path`
IPHONESIMULATOR_SYSROOT=`xcrun --sdk iphonesimulator --show-sdk-path`

# Uncomment if you want to see more information about each invocation
# of clang as the builds proceed.
# CLANG_VERBOSE="--verbose"

CC=clang
CXX=clang

CFLAGS="${CLANG_VERBOSE} -O2 -pipe -fPIC -fcxx-exceptions"
CXXFLAGS="${CLANG_VERBOSE} ${CFLAGS} -std=c++11 -stdlib=libc++"

LDFLAGS="-stdlib=libc++"
LIBS="-lc++ -lc++abi"

echo "PREFIX ..................... ${PREFIX}"
echo "USE_GIT_MASTER ............. ${USE_GIT_MASTER}"
echo "PROTOBUF_GIT_URL ........... ${PROTOBUF_GIT_URL}"
echo "PROTOBUF_GIT_DIRNAME ....... ${PROTOBUF_GIT_DIRNAME}"
echo "PROTOBUF_VERSION ........... ${PROTOBUF_VERSION}"
echo "PROTOBUF_RELEASE_URL ....... ${PROTOBUF_RELEASE_URL}"
echo "PROTOBUF_RELEASE_DIRNAME ... ${PROTOBUF_RELEASE_DIRNAME}"
echo "BUILD_MACOSX_X86_64 ........ ${BUILD_MACOSX_X86_64}"
echo "BUILD_I386_IOSSIM .......... ${BUILD_I386_IOSSIM}"
echo "BUILD_X86_64_IOSSIM ........ ${BUILD_X86_64_IOSSIM}"
echo "BUILD_IOS_ARMV7 ............ ${BUILD_IOS_ARMV7}"
echo "BUILD_IOS_ARMV7S ........... ${BUILD_IOS_ARMV7S}"
echo "BUILD_IOS_ARM64 ............ ${BUILD_IOS_ARM64}"
echo "PROTOBUF_SRC_DIR ........... ${PROTOBUF_SRC_DIR}"
echo "DARWIN ..................... ${DARWIN}"
echo "XCODEDIR ................... ${XCODEDIR}"
echo "IOS_SDK_VERSION ............ ${IOS_SDK_VERSION}"
echo "MIN_SDK_VERSION ............ ${MIN_SDK_VERSION}"
echo "MACOSX_PLATFORM ............ ${MACOSX_PLATFORM}"
echo "MACOSX_SYSROOT ............. ${MACOSX_SYSROOT}"
echo "IPHONEOS_PLATFORM .......... ${IPHONEOS_PLATFORM}"
echo "IPHONEOS_SYSROOT ........... ${IPHONEOS_SYSROOT}"
echo "IPHONESIMULATOR_PLATFORM ... ${IPHONESIMULATOR_PLATFORM}"
echo "IPHONESIMULATOR_SYSROOT .... ${IPHONESIMULATOR_SYSROOT}"
echo "CC ......................... ${CC}"
echo "CFLAGS ..................... ${CFLAGS}"
echo "CXX ........................ ${CXX}"
echo "CXXFLAGS ................... ${CXXFLAGS}"
echo "LDFLAGS .................... ${LDFLAGS}"
echo "LIBS ....................... ${LIBS}"

while true; do
    read -p "Proceed with build? (y/n) " yn
    case $yn in
        [Yy]* ) break;;
        [Nn]* ) exit;;
        * ) echo "Please answer yes or no.";;
    esac
done


###################################################################
# This section contains the build commands to create the native
# protobuf library for Mac OS X.  This is done first so we have
# a copy of the protoc compiler.  It will be used in all of the
# susequent iOS builds.
###################################################################

echo "$(tput setaf 2)"
echo "###################################################################"
echo "# x86_64 for Mac OS X"
echo "###################################################################"
echo "$(tput sgr0)"

if [ "${BUILD_MACOSX_X86_64}" == "YES" ]
then
    (
        cd ${PROTOBUF_SRC_DIR}
        make distclean
        ./configure --disable-shared --prefix=${PREFIX} --exec-prefix=${PREFIX}/platform/x86_64-mac "CC=${CC}" "CFLAGS=${CFLAGS} -arch x86_64" "CXX=${CXX}" "CXXFLAGS=${CXXFLAGS} -arch x86_64" "LDFLAGS=${LDFLAGS}" "LIBS=${LIBS}"
        make
        make check
        make install
    )
fi

PROTOC=${PREFIX}/platform/x86_64-mac/bin/protoc

###################################################################
# This section contains the build commands for each of the 
# architectures that will be included in the universal binaries.
###################################################################

echo "$(tput setaf 2)"
echo "###########################"
echo "# i386 for iPhone Simulator"
echo "###########################"
echo "$(tput sgr0)"

if [ "${BUILD_I386_IOSSIM}" == "YES" ]
then
    (
        cd ${PROTOBUF_SRC_DIR}
        make distclean
        ./configure --build=x86_64-apple-${DARWIN} --host=i386-apple-${DARWIN} --with-protoc=${PROTOC} --disable-shared --prefix=${PREFIX} --exec-prefix=${PREFIX}/platform/i386-sim "CC=${CC}" "CFLAGS=${CFLAGS} -miphoneos-version-min=${MIN_SDK_VERSION} -arch i386 -isysroot ${IPHONESIMULATOR_SYSROOT}" "CXX=${CXX}" "CXXFLAGS=${CXXFLAGS} -arch i386 -isysroot ${IPHONESIMULATOR_SYSROOT}" LDFLAGS="-arch i386 -miphoneos-version-min=${MIN_SDK_VERSION} ${LDFLAGS}" "LIBS=${LIBS}"
        make
        make install
    )
fi

echo "$(tput setaf 2)"
echo "#############################"
echo "# x86_64 for iPhone Simulator"
echo "#############################"
echo "$(tput sgr0)"

if [ "${BUILD_X86_64_IOSSIM}" == "YES" ]
then
    (
        cd ${PROTOBUF_SRC_DIR}
        make distclean
        ./configure --build=x86_64-apple-${DARWIN} --host=x86_64-apple-${DARWIN} --with-protoc=${PROTOC} --disable-shared --prefix=${PREFIX} --exec-prefix=${PREFIX}/platform/x86_64-sim "CC=${CC}" "CFLAGS=${CFLAGS} -miphoneos-version-min=${MIN_SDK_VERSION} -arch x86_64 -isysroot ${IPHONESIMULATOR_SYSROOT}" "CXX=${CXX}" "CXXFLAGS=${CXXFLAGS} -arch x86_64 -isysroot ${IPHONESIMULATOR_SYSROOT}" LDFLAGS="-arch x86_64 -miphoneos-version-min=${MIN_SDK_VERSION} ${LDFLAGS}" "LIBS=${LIBS}"
        make
        make install
    )
fi

echo "$(tput setaf 2)"
echo "##################"
echo "# armv7 for iPhone"
echo "##################"
echo "$(tput sgr0)"

if [ "${BUILD_IOS_ARMV7}" == "YES" ]
then
    (
        cd ${PROTOBUF_SRC_DIR}
        make distclean
        ./configure --build=x86_64-apple-${DARWIN} --host=armv7-apple-${DARWIN} --with-protoc=${PROTOC} --disable-shared --prefix=${PREFIX} --exec-prefix=${PREFIX}/platform/armv7-ios "CC=${CC}" "CFLAGS=${CFLAGS} -miphoneos-version-min=${MIN_SDK_VERSION} -arch armv7 -isysroot ${IPHONEOS_SYSROOT}" "CXX=${CXX}" "CXXFLAGS=${CXXFLAGS} -arch armv7 -isysroot ${IPHONEOS_SYSROOT}" LDFLAGS="-arch armv7 -miphoneos-version-min=${MIN_SDK_VERSION} ${LDFLAGS}" "LIBS=${LIBS}"
        make
        make install
    )
fi

echo "$(tput setaf 2)"
echo "###################"
echo "# armv7s for iPhone"
echo "###################"
echo "$(tput sgr0)"

if [ "${BUILD_IOS_ARMV7S}" == "YES" ]
then
    (
        cd ${PROTOBUF_SRC_DIR}
        make distclean
        ./configure --build=x86_64-apple-${DARWIN} --host=armv7s-apple-${DARWIN} --with-protoc=${PROTOC} --disable-shared --prefix=${PREFIX} --exec-prefix=${PREFIX}/platform/armv7s-ios "CC=${CC}" "CFLAGS=${CFLAGS} -miphoneos-version-min=${MIN_SDK_VERSION} -arch armv7s -isysroot ${IPHONEOS_SYSROOT}" "CXX=${CXX}" "CXXFLAGS=${CXXFLAGS} -arch armv7s -isysroot ${IPHONEOS_SYSROOT}" LDFLAGS="-arch armv7s -miphoneos-version-min=${MIN_SDK_VERSION} ${LDFLAGS}" "LIBS=${LIBS}"
        make
        make install
    )
fi

echo "$(tput setaf 2)"
echo "##################"
echo "# arm64 for iPhone"
echo "##################"
echo "$(tput sgr0)"

if [ "${BUILD_IOS_ARM64}" == "YES" ]
then
    (
        cd ${PROTOBUF_SRC_DIR}
        make distclean
        ./configure --build=x86_64-apple-${DARWIN} --host=arm --with-protoc=${PROTOC} --disable-shared --prefix=${PREFIX} --exec-prefix=${PREFIX}/platform/arm64-ios "CC=${CC}" "CFLAGS=${CFLAGS} -miphoneos-version-min=${MIN_SDK_VERSION} -arch arm64 -target arm64-apple-ios10.0 -isysroot ${IPHONEOS_SYSROOT}" "CXX=${CXX}" "CXXFLAGS=${CXXFLAGS} -arch arm64 -target arm64-apple-ios10.0 -isysroot ${IPHONEOS_SYSROOT}" LDFLAGS="-arch arm64 -miphoneos-version-min=${MIN_SDK_VERSION} ${LDFLAGS}" "LIBS=${LIBS}"
        make
        make install
    )
fi


echo Done!

