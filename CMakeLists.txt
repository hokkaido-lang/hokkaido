cmake_minimum_required(VERSION 3.10)
project(my_compiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# LLVM cmake config references ZLIB::ZLIB; satisfy it with a minimal imported target
find_path(ZLIB_INCLUDE_DIRS zlib.h)
find_library(ZLIB_LIBRARIES NAMES z)
if(ZLIB_INCLUDE_DIRS AND ZLIB_LIBRARIES)
  add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
  set_target_properties(ZLIB::ZLIB PROPERTIES
    IMPORTED_LOCATION "${ZLIB_LIBRARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIRS}"
  )
endif()

find_package(LLVM REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs
    support
    core
    irreader
    codegen
    target
    mc
    mcparser
    asmparser
    option
    WebAssembly
    ${LLVM_TARGETS_TO_BUILD}
)

# Build the Rust cubical library as a static library
set(CUBICAL_LIB "${CMAKE_SOURCE_DIR}/target/release/libcubical_c.a")

# Custom target to build the Rust library
add_custom_target(cubical_lib ALL
    COMMAND cargo build --release
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building cubical Rust library..."
    BYPRODUCTS ${CUBICAL_LIB}
)

# Explicit list of source files in src-cpp/
set(HOKKAIDO_SOURCES
    src-cpp/main.cpp
    src-cpp/cubical.cpp
    src-cpp/lexer.cpp
    src-cpp/parser.cpp
    src-cpp/parser_types.cpp
    src-cpp/parser_decls.cpp
    src-cpp/parser_stmts.cpp
    src-cpp/parser_exprs.cpp
    src-cpp/codegen.cpp
    src-cpp/codegen_types.cpp
    src-cpp/codegen_expr.cpp
    src-cpp/codegen_top.cpp
    src-cpp/codegen_init.cpp
    src-cpp/codegen_decl.cpp
    src-cpp/codegen_stmt.cpp
    src-cpp/codegen_pattern.cpp
    src-cpp/borrow_checker.cpp
)

add_executable(hokkaido ${HOKKAIDO_SOURCES})

# Add dependency on the cubical library
add_dependencies(hokkaido cubical_lib)

target_link_libraries(hokkaido PRIVATE
    ${llvm_libs}
    ZLIB::ZLIB
    ${CUBICAL_LIB}
    pthread
    dl
    m
)

# LSP server — separate binary
set(HOK_LSP_SOURCES
    src-cpp/lsp_main.cpp
    src-cpp/lsp/lsp.cpp
    src-cpp/lexer.cpp
    src-cpp/parser.cpp
    src-cpp/parser_types.cpp
    src-cpp/parser_decls.cpp
    src-cpp/parser_stmts.cpp
    src-cpp/parser_exprs.cpp
)

add_executable(hok-lsp ${HOK_LSP_SOURCES})

target_include_directories(hok-lsp PRIVATE src-cpp)

target_link_libraries(hok-lsp PRIVATE
    ${llvm_libs}
    pthread
    dl
    m
)