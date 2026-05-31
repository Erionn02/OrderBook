set -e
build_type="Debug"
build_dir="cmake-build-debug"
if [ "$1" == "Release" ];
then
    build_type="Release"
    build_dir="cmake-build-release"
fi
mkdir -p $build_dir
conan install . --output-folder=$build_dir --build=missing -s build_type=$build_type
cd $build_dir && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=$build_type -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake && cmake --build . -- -j $(nproc --all)