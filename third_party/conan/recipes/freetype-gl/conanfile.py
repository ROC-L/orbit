from conans import ConanFile, CMake, tools
import shutil, os


class FreetypeglConan(ConanFile):
    name = "freetype-gl"
    version = "79b03d9"
    license = "BSD-2-Clause"
    description = "freetype-gl is a small library for displaying Unicode in OpenGL"
    topics = ("freetype", "opengl", "unicode", "fonts")
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC" : [True, False]}
    default_options = {"shared": False, "fPIC": True}
    generators = "cmake", "cmake_find_package_multi"
    requires = "glad/0.1.34", "freetype/2.10.4", "zlib/1.2.11"
    exports_sources = "patches/*"
    _cmake = None

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC


    def source(self):
        self.run("git clone https://github.com/rougier/freetype-gl.git")
        self.run("git checkout {}".format(self.version), cwd="freetype-gl/")
        tools.patch(base_path="freetype-gl/", patch_file="patches/001-patch.diff")

    def _get_cmake(self):
        if self._cmake:
            return self._cmake

        cmake = CMake(self)
        cmake.definitions["freetype-gl_WITH_GLAD"] = True
        cmake.definitions["freetype-gl_WITH_GLEW"] = False
        cmake.definitions["freetype-gl_USE_VAO"] = False
        cmake.definitions["freetype-gl_BUILD_DEMOS"] = False
        cmake.definitions["freetype-gl_BUILD_APIDOC"] = False
        cmake.definitions["freetype-gl_BUILD_HARFBUZZ"] = False
        cmake.definitions["freetype-gl_BUILD_MAKEFONT"] = False
        cmake.definitions["freetype-gl_BUILD_TESTS"] = False
        cmake.configure(source_folder="freetype-gl")
        self._cmake = cmake
        return cmake

    def build(self):
        if "mesa" in self.deps_cpp_info.deps:
            mesa_path = self.deps_cpp_info["mesa"].rootpath
            shutil.copyfile(os.path.join(mesa_path, "lib", "pkgconfig", "gl.pc"), "gl.pc")
            tools.replace_prefix_in_pc_file("gl.pc", mesa_path)

        with tools.environment_append({"PKG_CONFIG_PATH": os.getcwd()}):
            cmake = self._get_cmake()
            cmake.build()

    def package(self):
        cmake = self._get_cmake()
        cmake.install()
        self.copy("*", src="freetype-gl/fonts/", dst="fonts/")
        self.copy("*", src="freetype-gl/shaders/", dst="shaders/")

    def package_info(self):
        if self.settings.os == "Windows":
            self.cpp_info.libs = ["freetype-gl.lib"]
        else:
            self.cpp_info.libs = ["libfreetype-gl.a"]
        self.cpp_info.includedirs = ["include"]
        self.cpp_info.resdirs = ["fonts", "shaders"]
        self.cpp_info.defines = ["GL_WITH_GLAD"]

