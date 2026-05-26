set -e
build_dir="cmake-build-debug"
if [ "$1" ];
then
    build_dir=$1
fi
mkdir -p $build_dir
conan install . --output-folder=$build_dir --build=missing
conan install . --output-folder=$build_dir --build=missing -s build_type=Debug
cd $build_dir && cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake && cmake --build . -- -j $(nproc --all)