set -e
cd deps
bash build_unwind.sh
bash build_dwarf.sh
cd ..

export CPATH=$CPATH:$(pwd)/deps/libdwarf/build:$(pwd)/deps/libdwarf
export CPATH=$CPATH:$(pwd)/deps/libunwind/install/usr/local/include

export LIBRARY_PATH=$LIBRARY_PATH:$(pwd)/deps/libunwind/install/usr/local/lib
export LIBRARY_PATH=$LIBRARY_PATH:$(pwd)/deps/libdwarf/build/libdwarf

export CC=gcc
export CXX=g++

# Configure all dependencies
echo BUILDIT_DIR=$(pwd)/buildit > d2x/Makefile.inc
echo D2X_PATH=$(pwd)/d2x > buildit/Makefile.inc
echo D2X_DEBUGGING=1 >> buildit/Makefile.inc
sed -i 's/-lelf//g' buildit/Makefile
sed -i 's/-ldl/-ldl -lelf/g' buildit/Makefile
sed -i 's/ elf//g' graphit/CMakeLists.txt
sed -i 's/dl)/dl elf)/g' graphit/CMakeLists.txt

mkdir -p build

# First build D2X library
make -C d2x/ -j$(nproc) lib

# Next build BuildIt with D2X support
make -C buildit/ -j$(nproc)

# Finally build graphit with BuildIt and D2X
D2X_DIR=$(pwd)/d2x
BUILDIT_DIR=$(pwd)/buildit
cd graphit
mkdir -p build
cd build
cmake -DD2X_DIR=$D2X_DIR -DBUILDIT_DIR=$BUILDIT_DIR ..
make -j$(nproc)
cd ../..


