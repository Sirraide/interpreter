cmake_minimum_required(VERSION 3.25)
project(interpreter VERSION 0.1.0 LANGUAGES CXX)

## ============================================================================
##  Global CMake Variables.
## ============================================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})
set(CMAKE_CXX_STANDARD_REQUIRED ON)

## ============================================================================
##  Global compiler options.
## ============================================================================
## Turn on diagnostics colours.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
endif()

## ============================================================================
##  Compiler options.
## ============================================================================
add_library(options INTERFACE)

## Flags for Clang and GCC.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(options INTERFACE
        ## Warnings.
        -Wall -Wextra     # Enable ‘all’ warnings.
        -Wundef           # Invalid #undef or undefined macro in #if.
        -Wshadow          # Shadowing.
        -Wcast-align      # Casting that changes alignment.
        -Wconversion      # Implicit conversions.
        -Wsign-conversion # Implicit sign conversions.
        -Wlogical-op      # Duplicate or unintended logical operators.
        -Wformat=2        # Stricter format checking.

        ## Disabled warnings.
        -Wno-unused-function
        -Wno-unused-local-typedefs
        -Wno-unused-parameter

        ## NULL Errors.
        -Werror=null-dereference
        -Werror=nonnull # Passing NULL to nonnull parameter.

        ## Memory Errors.
        -Werror=address              # Suspicious use of addresses.
        -Werror=init-self            # Initialization of a variable with itself.
        -Werror=invalid-memory-model # For atomics.
        -Werror=maybe-uninitialized
        -Werror=uninitialized
        -Werror=return-local-addr

        ## Return type.
        -Werror=return-type
        -Werror=missing-noreturn

        ## C/C++.
        -Werror=implicit-fallthrough
        -Werror=missing-include-dirs # User-specified include dir does not exist.
        -Werror=pointer-arith        # Disallow void* and function pointer arithmetic.
        -Werror=string-compare       # Nonsensical string comparisons.
        -Werror=switch               # Missing switch cases.
        # -Werror=switch-enum          # Switch on enum (even if there is a default case).
        -Werror=write-strings        # Strings in C should be const char*.

        ## C++.
        -Werror=missing-field-initializers
        -Werror=missing-requires
        -Werror=non-virtual-dtor
        -Werror=pessimizing-move
    )
endif()

## Additional flags for Clang.
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(options INTERFACE -Werror=dangling )
endif()

## Flags for MSVC.
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(options INTERFACE
        /W4 # Enable ‘all’ warnings.

        # Allow unnamed structs/unions.
        /wd4201

        # Don’t suggest the _s nonsense functions.
        /D_CRT_SECURE_NO_WARNINGS
        /D_CRT_SECURE_NO_WARNINGS_GLOBALS
        /D_CRT_NONSTDC_NO_WARNINGS
    )
endif()

## Debug/Release flags.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(options INTERFACE
        $<$<CONFIG:DEBUG>:-O0 -g3 -ggdb3 -rdynamic -DINTERP_DEBUG>
        $<$<CONFIG:RELEASE>:-O3 -march=native -mtune-native>
    )
    target_link_options(options INTERFACE
        $<$<CONFIG:DEBUG>:-O0 -g3 -ggdb3 -rdynamic -DINTERP_DEBUG>
        $<$<CONFIG:RELEASE>:-O3 -march=native -mtune-native>
    )
else()
    target_compile_options(options INTERFACE
        $<$<CONFIG:DEBUG>:/Od /DINTERP_DEBUG>
        $<$<CONFIG:RELEASE>:/O2>
    )
endif()

## ============================================================================
##  Submodules and include dirs.
## ============================================================================
## Link against libfmt.
target_link_libraries(options INTERFACE fmt)

## ‘src’ should be an include directory.
target_include_directories(options INTERFACE src)

## ‘include’ too, if it exists.
if (EXISTS ${PROJECT_SOURCE_DIR}/include)
    target_include_directories(options INTERFACE include)
endif()

## As well as everything in ‘libs’.
if (EXISTS ${PROJECT_SOURCE_DIR}/libs)
    file(GLOB libs ${PROJECT_SOURCE_DIR}/libs/*)

    foreach(lib ${libs})
        if (IS_DIRECTORY ${lib}/include)
            target_include_directories(options INTERFACE ${lib}/include)
        endif()
    endforeach()

    ## Also add all of them as subdirectories if they have a CMakeLists.txt.
    foreach (lib ${libs})
        if (EXISTS ${lib}/CMakeLists.txt)
            add_subdirectory(${lib})
        endif()
    endforeach()
endif()

## ============================================================================
##  Executables and libraries.
## ============================================================================
## Everything in ‘src’ is part of the project.
file(GLOB_RECURSE LIB src/*.cc src/*.hh)
file(GLOB_RECURSE EXAMPLE example/*.cc example/*.hh)

## As well as everything in ‘include’, if it exists.
if (EXISTS ${PROJECT_SOURCE_DIR}/include)
    file(GLOB_RECURSE includes include/*.hh)
    list(APPEND LIB ${includes})
endif()

## Add the library and executable.
add_library(interpreter STATIC ${LIB})
add_executable(test ${EXAMPLE})

## Apply our options.
target_link_libraries(interpreter PRIVATE options)
target_link_libraries(test PRIVATE options interpreter)

## Make sure transitive dependencies are handled properly.
target_include_directories(interpreter PUBLIC include)
target_link_libraries(interpreter PUBLIC fmt)