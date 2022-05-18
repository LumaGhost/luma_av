FROM gcc:12

RUN apt-get update
RUN apt-get install -y \
    python3-pip \
    cmake

RUN pip3 install ninja

RUN git clone https://github.com/llvm/llvm-project.git && \
    cd llvm-project && \
    git checkout llvmorg-14.0.3 && \
    cmake -S llvm -B build -G "Ninja" -DLLVM_ENABLE_PROJECTS="clang;libcxx;libcxxabi;clang-tools-extra;compiler-rt;lld" -DCMAKE_BUILD_TYPE="Release" && \
    cmake --build build && \
    cmake --install build --prefix /llvm-install/ && \
    cp /llvm-project/clang-tools-extra/clang-tidy/tool/run-clang-tidy.py /llvm-install/scripts && \
    cp /llvm-project/clang-tools-extra/clang-tidy/tool/clang-tidy-diff.py /llvm-install/scripts && \
    cp /llvm-project/clang/tools/clang-format/clang-format-diff.py /llvm-install/scripts && \
    cp /llvm-project/clang/tools/clang-format/clang-format.py /llvm-install/scripts && \
    rm -rf /llvm-project/ 

RUN pip3 install conan && \
    conan profile new --detect default && \
    conan profile update settings.compiler.libcxx=libstdc++11 default && \
    conan config set general.revisions_enabled=1

RUN conan profile new clang-libcxx --detect && \
    conan profile update settings.compiler.libcxx=libc++ clang-libcxx  && \
    conan profile update settings.compiler=clang clang-libcxx  && \
    conan profile update settings.compiler.version=14 clang-libcxx  && \
    conan profile update env.CC=/llvm-install/bin/clang clang-libcxx  && \
    conan profile update env.CXX=/llvm-install/bin/clang++ clang-libcxx 

ADD conanfile.py /docker_conan_install/
ENV CONAN_SYSREQUIRES_SUDO=False
ENV CONAN_SYSREQUIRES_MODE=enabled
RUN conan install /docker_conan_install/ --profile clang-libcxx -if build --build missing -s build_type=Release && \
    rm -rf /build/
RUN rm -rf /docker_conan_install/

