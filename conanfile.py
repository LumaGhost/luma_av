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
        "shared": True,
        "ffmpeg:with_opus": False,
        "ffmpeg:with_libalsa": False,
        "ffmpeg:with_libx265": False,
        "ffmpeg:with_libmp3lame": False,
        "ffmpeg:with_pulse": False,
        "ffmpeg:shared": True,
        "gtest:build_gmock": False,
        "gtest:shared": True,
    }
    generators = "CMakeToolchain", "CMakeDeps"
    exports_sources = "src/*", "test/*", "CMakeLists.txt"

    def requirements(self):
        self.requires("outcome/2.1.5")
        self.requires("ffmpeg/4.4")
        self.requires("gtest/cci.20210126")

    def build(self):
        cmake = CMake(self)
        cmake.definitions["CMAKE_TOOLCHAIN_FILE"] = "conan_toolchain.cmake"
        cmake.configure()
        cmake.build()

    def package(self):
        self.copy("*.hpp", dst="include", src="hello")
        self.copy("*.so", dst="lib", keep_path=False)
        self.copy("*.a", dst="lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["luma_av"]

