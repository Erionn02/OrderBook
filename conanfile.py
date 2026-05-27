from conan import ConanFile
from conan.tools.cmake import CMakeToolchain

class TemplateProjectConan(ConanFile):
    name = "TemplateProject/1.0"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    requires = "gtest/1.17.0"

    def configure(self):
        pass

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generator = "Ninja"
        tc.generate()