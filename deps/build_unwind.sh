set -e
cd libunwind 
autoreconf -i
./configure
make 
mkdir -p install
mkdir -p install/usr/
mkdir -p install/usr/local
make install prefix=$(pwd)/install/usr/local

cd ..
