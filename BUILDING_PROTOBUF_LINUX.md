
git clone --recursive -j8 https://github.com/protocolbuffers/protobuf.git

cd protobuf

// remove some untracked files before below command
git checkout tags/v3.21.6 -b v3.21.6

mkdir buildLinux
cd buildLinux

// Release
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -Dprotobuf_BUILD_TESTS=OFF -DCMAKE_CXX_FLAGS="-fpic -O2" -DCMAKE_POSITION_INDEPENDENT_CODE=ON ..

// Debug
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -Dprotobuf_BUILD_TESTS=OFF -DCMAKE_CXX_FLAGS="-fpic -O2" -DCMAKE_POSITION_INDEPENDENT_CODE=ON ..

// compile
cmake --build . --parallel 10

// install
sudo cmake --install .