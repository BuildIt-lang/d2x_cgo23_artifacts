set -e
cd libdwarf
rm -rf build
mkdir build
cd build
mkdir -p install
mkdir -p install/usr/
mkdir -p install/usr/local
../configure --enable-shared --disable-nonshared
make
make install prefix=$(pwd)/install/usr/local
