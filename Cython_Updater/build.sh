python3 setup.py build_ext --inplace
cmake -DCMAKE_PREFIX_PATH=`pwd`/../libtorch-cpu -B build
cd build
make app
