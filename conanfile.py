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
        "ffmpeg:with_opus": False,
        "ffmpeg:with_libalsa": False,
        "gtest:build_gmock": False
    }
    generators = "cmake", "cmake_find_package"
    exports_sources = "src/*", "test/*", "CMakeLists.txt"

    def requirements(self):
        self.requires("outcome/2.1.5")
        self.requires("ffmpeg/4.4")
        self.requires("zlib/1.2.11@conan/stable")
        self.requires("bzip2/1.0.8@conan/stable")
        self.requires("gtest/1.11.0")

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

