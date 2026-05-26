set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -w -O0 -g3 -no-pie")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_DEBUG} -w -O0 -g3 -no-pie")

package_add_test(exampleUnitTest
        SOURCES ExampleUnitTest.cpp
        DEPENDS template_lib_one
        )

package_add_test(exampleUnitTest2
        SOURCES ExampleUnitTest2.cpp
        DEPENDS template_lib_two
        )
