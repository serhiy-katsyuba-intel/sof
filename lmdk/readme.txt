To build a sample loadable library:

cd libraries/sample
mkdir build
cd build

cmake -DCMAKE_VERBOSE_MAKEFILE=ON ..
cmake --build .