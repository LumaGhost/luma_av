from conans import ConanFile, CMake, tools


class LumaAvConan(ConanFile):
    name = "luma_av"
    version = "0.0.0"
    license = "<Put the package license here>"
    author = "<Put your name here> <And your email here>"
    url = "<Package recipe repository url here, for issues about the package>"
    description = "<Description of Lumaav here>"
    topics = ("<Put some tag here>", "<here>", "<and here>")
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = {
        "shared": False,
        "ffmpeg:xcb": False,
        "ffmpeg:opus": False,
        "ffmpeg:alsa": False
    }
    generators = "cmake_find_package"
    exports_sources = "src/*", "test/*", "CMakeLists.txt"

    def requirements(self):
        self.requires("boost/1.71.0@conan/stable")
        self.requires("ffmpeg/4.2.1@bincrafters/stable")
        self.requires("zlib/1.2.11@conan/stable")
        self.requires("bzip2/1.0.8@conan/stable")
        self.requires("gtest/1.8.1")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        self.copy("*.hpp", dst="include", src="hello")
        self.copy("*.so", dst="lib", keep_path=False)
        self.copy("*.a", dst="lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["luma_av"]

