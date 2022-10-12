1) Create the toolchain using
```Bash
~/Android/Sdk/ndk/android-ndk-r15c/build/tools/make-standalone-toolchain.sh \
	--arch=arm64 \
	--platform=android-21 \
	--toolchain=arm-linux-android-clang5.0 \
	--install-dir=$HOME/Android/arm-21-toolchain-clang \
	--use-llvm \
	--stl=libc++
```

2) Get Protobuf
```Bash
git clone --recursive https://github.com/protocolbuffers/protobuf.git
cd protobuf
git checkout v3.21.6
./autogen.sh
mkdir build && cd build
nano build-protobuf-android.sh
```   

3) Fill the contents of `build-protobuf-android.sh`
```Bash
export PREFIX=$HOME/Workspace/protobuf
export PATH=$HOME/Android/arm-21-toolchain-clang/bin:$PATH
export SYSROOT=$HOME/Android/arm-21-toolchain-clang/sysroot
export CC="aarch64-linux-android-clang --sysroot $SYSROOT"
export CXX="aarch64-linux-android-clang++ --sysroot $SYSROOT"
   
../configure \
   --prefix=$PREFIX \
   --host=arm-linux-androideabi \
   --with-sysroot="${SYSROOT}" \
   --enable-shared \
   --enable-cross-compile \
   --with-protoc=protoc \
   CFLAGS="-march=armv8-a -O2 -D__ANDROID_API__=21" \
   CXXFLAGS="-frtti -fexceptions -march=armv8-a -O2 -D__ANDROID_API__=21" \
   LIBS="-llog -lz -lc++_static"
   
make -j 2
   
make install
```

4) Build
```Bash
bash build-protobuf-android.sh
```

5) There is a folder called  `lib`  inside where you cloned protobuf, get the compiled libs from there.
