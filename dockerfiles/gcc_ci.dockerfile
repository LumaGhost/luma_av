FROM gcc:12

RUN apt update && apt install -y \
    python3-pip \
    cmake

RUN pip3 install conan

RUN conan profile new --detect default && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

ADD conanfile.py /docker_conan_install/
ENV CONAN_SYSREQUIRES_SUDO=False
ENV CONAN_SYSREQUIRES_MODE=enabled
RUN conan install /docker_conan_install/ -if build --build missing -s build_type=Release && \
    rm -rf /build/
RUN rm -rf /docker_conan_install/

