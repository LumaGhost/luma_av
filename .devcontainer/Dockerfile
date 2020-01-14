FROM gcc:9.2

RUN apt-get update && apt install python3-pip -y
RUN pip3 install conan
RUN conan remote add bintray https://api.bintray.com/conan/conan-community/conan
RUN apt install cmake -y
RUN conan install boost/1.71.0@conan/stable --build=missing -s compiler.libcxx=libstdc++11
RUN conan install gtest/1.8.0@conan/stable --build=missing -s compiler.libcxx=libstdc++11

RUN apt-get install clang-tools-7 -y && update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-7 100

RUN conan remote add bincrafters https://api.bintray.com/conan/bincrafters/public-conan
RUN conan install ffmpeg/4.2.1@bincrafters/stable --build=missing -s compiler.libcxx=libstdc++11