if [ ! -d "build" ]; then
    cmake -B build -DCMAKE_BUILD_TYPE=Debug
fi
cmake --build build --parallel
