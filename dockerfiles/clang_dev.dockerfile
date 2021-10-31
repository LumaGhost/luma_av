FROM gcc:11.2.0

RUN apt-get update
RUN apt-get install -y \
    python3-pip \
    cmake

RUN pip3 install ninja

RUN git clone https://github.com/llvm/llvm-project.git && \
    cd llvm-project && \
    git checkout llvmorg-13.0.0 && \
    cmake -S llvm -B build -G "Ninja" -DLLVM_ENABLE_PROJECTS="clang;libcxx;libcxxabi;clang-tools-extra;compiler-rt;lld;libunwind;lldb;polly;debuginfo-tests" -DCMAKE_BUILD_TYPE="Release" && \
    cmake --build build -j 8

RUN apt-get install -y \
    gdb

RUN pip3 install conan && \
    conan remote add bincrafters https://bincrafters.jfrog.io/artifactory/api/conan/public-conan && \
    conan profile new --detect default && \
    conan profile update settings.compiler.libcxx=libstdc++11 default && \
    conan config set general.revisions_enabled=1

RUN conan profile new clang --detect && \
    conan profile update settings.compiler.libcxx=libstdc++11 clang && \
    conan profile update settings.compiler=clang clang && \
    conan profile update settings.compiler.version=13 clang && \
    conan profile update env.CC=/llvm-project/build/bin/clang clang && \
    conan profile update env.CXX=/llvm-project/build/bin/clang++ clang

ENV CONAN_SYSREQUIRES_SUDO=False