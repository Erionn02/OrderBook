from conan import ConanFile

class TemplateProjectConan(ConanFile):
    name = "TemplateProject/1.0"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"
    requires = "gtest/1.17.0"

    def configure(self):
        pass
