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

RUN conan install ffmpeg/4.2.1@bincrafters/stable --build=missing -s compiler.libcxx=libstdc++11
RUN conan remote add bintray https://api.bintray.com/conan/conan-community/conan
RUN conan install gtest/1.8.0@conan/stable --build=missing -s compiler.libcxx=libstdc++11
RUN conan install boost/1.71.0@conan/stable --build=missing -s compiler.libcxx=libstdc++11
RUN conan install zlib/1.2.11@conan/stable --build=missing -s compiler.libcxx=libstdc++11
RUN conan install bzip2/1.0.8@conan/stable --build=missing -s compiler.libcxx=libstdc++11
