
git clone --recursive -j8 https://github.com/ValveSoftware/GameNetworkingSockets.git

cd GameNetworkingSockets

git checkout tags/v1.4.1 -b v1.4.1

mkdir buildLinux
cd buildLinux

// Check CMakeLists.txt at the root GameNetworkingSockets to set configs like P2P, static Protobuf etc.

// Release
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -G Ninja ..
// Debug
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -G Ninja ..

// compile
ninja
