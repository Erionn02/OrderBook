include(CMakePrintHelpers)

function(package_add_test TESTNAME)
    cmake_parse_arguments(ARGS "" "" "SOURCES;DEPENDS" ${ARGN})
    add_executable(${TESTNAME} ${ARGS_SOURCES})
    target_include_directories(${TESTNAME} PUBLIC ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(
            ${TESTNAME}
            ${PROTO_GENERATED_LIB}
            ${CONAN_LIBS}
            ${CMAKE_DL_LIBS}
            ${ARGS_DEPENDS}
    )
    gtest_discover_tests(${TESTNAME}
            WORKING_DIRECTORY ${PROJECT_DIR}
            PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_DIR}"
    )
    set_target_properties(${TESTNAME} PROPERTIES FOLDER ${CMAKE_SOURCE_DIR}/tests)
endfunction()

function(add_app APP_NAME)
    add_executable(${APP_NAME} ${ARGN})
    get_property(PROJECT_LIBS GLOBAL PROPERTY PROJECT_LIBS_PROPERTY)
    target_link_libraries(${APP_NAME} PRIVATE "${PROJECT_LIBS}")
    set_link_options(${APP_NAME})
endfunction()

function(add_lib LIB_NAME)
    cmake_parse_arguments(ARGS "" "" "SOURCES;DEPENDS" ${ARGN})
    add_library(${LIB_NAME} ${ARGS_SOURCES})
    get_property(PROJECT_LIBS GLOBAL PROPERTY PROJECT_LIBS_PROPERTY)
    set_property(GLOBAL PROPERTY PROJECT_LIBS_PROPERTY "${PROJECT_LIBS};${LIB_NAME}")
    target_link_libraries(${LIB_NAME} PRIVATE "${ARGS_DEPENDS}")
    set_link_options(${LIB_NAME})
endfunction()

function(set_link_options TARGET_NAME)
    target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(${TARGET_NAME} PUBLIC ${PROTO_GENERATED_LIB} ${CONAN_LIBS})
    target_compile_options(${TARGET_NAME} PRIVATE
            -Wno-unused-variable
            -Wno-maybe-uninitialized
            -Werror
            -Wall
            -Wextra
            -Wnon-virtual-dtor
            -Wcast-align
            -Wunused
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wmisleading-indentation
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
            -Wnull-dereference
            -Wuseless-cast
            -Wdouble-promotion
            -freflection
            -march=native
    )
endfunction()

macro(add_conan_lib LIB)
    find_package(${LIB} REQUIRED)
    include_directories(SYSTEM ${${LIB}_INCLUDE_DIRS})
    foreach(_conan_target ${${LIB}_LIBRARIES})
        if(TARGET ${_conan_target})
            set_target_properties(${_conan_target} PROPERTIES SYSTEM TRUE)
        endif()
    endforeach()
    list(APPEND CONAN_LIBS "${${LIB}_LIBRARIES}")
endmacro()

function(setup_proto)
    message("Generating proto files...")
    file(GLOB PROTO_FILES CONFIGURE_DEPENDS proto/*.proto)

    set(GENERATED_PROTO_DIR "${CMAKE_BINARY_DIR}/proto")
    file(MAKE_DIRECTORY ${GENERATED_PROTO_DIR})

    if(${gRPC_FOUND})
        MESSAGE("gRPC found, generating for GRPC as well")
        execute_process(COMMAND ${Protobuf_PROTOC_EXECUTABLE} --proto_path=${CMAKE_SOURCE_DIR}/proto --cpp_out=${GENERATED_PROTO_DIR} --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_PROGRAM} --grpc_out ${GENERATED_PROTO_DIR} ${PROTO_FILES} RESULT_VARIABLE ret)
    else()
        execute_process(COMMAND ${Protobuf_PROTOC_EXECUTABLE} --proto_path=${CMAKE_SOURCE_DIR}/proto --cpp_out=${GENERATED_PROTO_DIR} ${PROTO_FILES} RESULT_VARIABLE ret)
    endif()

    if(NOT ret EQUAL 0)
        message( FATAL_ERROR "Protoc: bad exit status: ${ret}")
    endif()
    include_directories(SYSTEM ${GENERATED_PROTO_DIR})
    file(GLOB PROTO_SRC ${GENERATED_PROTO_DIR}/*.cc)
    add_library(PROTO_GENERATED ${PROTO_SRC})
    set(PROTO_GENERATED_LIB PROTO_GENERATED PARENT_SCOPE)
endfunction()