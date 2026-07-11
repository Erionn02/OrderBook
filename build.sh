set -e
build_type="Release"
build_dir="cmake-build-release"
if [ $# -ne 0 ];
then
    build_type=$1
    build_dir="cmake-build-$(echo "$1" | awk '{print tolower($0)}')"
fi
mkdir -p $build_dir
conan install . --output-folder=$build_dir --build=missing -s build_type=$build_type
cd $build_dir && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=$build_type "${@:2}" -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake && cmake --build . -- -j $(nproc --all)