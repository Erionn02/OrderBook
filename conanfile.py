from conan import ConanFile
from conan.tools.cmake import CMakeToolchain

class OrderBook(ConanFile):
    name = "OrderBook/1.0"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    requires = "gtest/1.17.0", "benchmark/1.9.5", "asio/1.38.0", "boost/1.91.0"
    default_options = {
        "boost/*:header_only": True
    }

    def configure(self):
        pass

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generator = "Ninja"
        tc.generate()