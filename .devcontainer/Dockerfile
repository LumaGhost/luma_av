FROM gcc:9.2

RUN apt update && apt install -y \
    python3-pip \
    cmake \
    clang-7 \
    clang-tools-7 && \
    pip3 install compdb && \
    pip3 install conan && \
    conan remote add bincrafters https://api.bintray.com/conan/bincrafters/public-conan && \
    conan profile new --detect default && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

ENV CONAN_SYSREQUIRES_SUDO=False