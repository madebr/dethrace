from conans import ConanFile, CMake
from conan.tools.files import replace_in_file
import glob
import os


class CarmaBypass(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    generators = "cmake", "cmake_find_package"
    default_options = {
        "ulfius:enable_websockets": False,
        "ulfius:with_gnutls": False,
        "ulfius:with_jansson": True,
        "ulfius:with_libcurl": False,
        "ulfius:with_yder": True,
    }

    def requirements(self):
        self.requires("detours/cci.20220630")
        self.requires("ulfius/2.7.11")

    def validate(self):
        assert self.settings.os == "Windows"
        assert self.settings.arch == "x86"
        assert self.settings.build_type == "Release"

    def build(self):
        detours_find_cmake = os.path.join(self.build_folder, "Finddetours.cmake")
        if os.path.exists(detours_find_cmake):
            os.unlink(detours_find_cmake)

        replace_in_file(self, os.path.join(self.build_folder, "FindUlfius.cmake"),
                        "Ulfius::Ulfius ", "Ulfius::Ulfius-static ", strict=False)

        cmake = CMake(self)
        cmake.definitions["HOOK_WEBSERVER"] = True
        cmake.generator = "Ninja"
        cmake.configure()
        cmake.build()
