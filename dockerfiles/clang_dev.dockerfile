FROM gcc:11.3.0

RUN apt-get update
RUN apt-get install -y \
    python3-pip \
    cmake

RUN pip3 install ninja

RUN git clone https://github.com/llvm/llvm-project.git
RUN cd llvm-project && \
    git checkout llvmorg-14.0.3 && \
    cmake -S llvm -B build -G "Ninja" -DLLVM_ENABLE_PROJECTS="clang;libcxx;libcxxabi;clang-tools-extra;compiler-rt;lld;libunwind;lldb;polly" -DCMAKE_BUILD_TYPE="Release" && \
    cmake --build build

RUN apt-get install -y \
    gdb

RUN pip3 install conan && \
    conan remote add bincrafters https://bincrafters.jfrog.io/artifactory/api/conan/public-conan && \
    conan profile new --detect default && \
    conan profile update settings.compiler.libcxx=libstdc++11 default && \
    conan config set general.revisions_enabled=1

RUN conan profile new clang-stdcxx --detect && \
    conan profile update settings.compiler.libcxx=libstdc++11 clang-stdcxx && \
    conan profile update settings.compiler=clang clang-stdcxx && \
    conan profile update settings.compiler.version=14 clang-stdcxx && \
    conan profile update env.CC=/llvm-project/build/bin/clang clang-stdcxx && \
    conan profile update env.CXX=/llvm-project/build/bin/clang++ clang-stdcxx

RUN conan profile new clang-libcxx --detect && \
    conan profile update settings.compiler.libcxx=libc++ clang-libcxx  && \
    conan profile update settings.compiler=clang clang-libcxx  && \
    conan profile update settings.compiler.version=14 clang-libcxx  && \
    conan profile update env.CC=/llvm-project/build/bin/clang clang-libcxx  && \
    conan profile update env.CXX=/llvm-project/build/bin/clang++ clang-libcxx 

ENV CONAN_SYSREQUIRES_SUDO=False
ENV CONAN_SYSREQUIRES_MODE=enabled