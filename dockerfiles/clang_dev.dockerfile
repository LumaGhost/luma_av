FROM lumauwu/luma_clang_ci:latest

RUN apt-get install -y \
    gdb

RUN conan profile new clang-stdcxx --detect && \
    conan profile update settings.compiler.libcxx=libstdc++11 clang-stdcxx && \
    conan profile update settings.compiler=clang clang-stdcxx && \
    conan profile update settings.compiler.version=14 clang-stdcxx && \
    conan profile update env.CC=/llvm-install/bin/clang clang-stdcxx && \
    conan profile update env.CXX=/llvm-install/bin/clang++ clang-stdcxx