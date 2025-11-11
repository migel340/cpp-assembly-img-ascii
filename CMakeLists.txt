cmake_minimum_required(VERSION 3.15)
project(img_to_ascii LANGUAGES CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable threading
find_package(Threads REQUIRED)

# Optimization flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O2")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")

enable_language(ASM)

add_executable(
        img_to_ascii
        src/main.cpp
        src/image_loader.cpp
        src/image_converter.cpp
        first_arm_function.asm
)

# Dodaj katalog include do ścieżek nagłówków
target_include_directories(img_to_ascii PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Link threading library
target_link_libraries(img_to_ascii PRIVATE Threads::Threads)

# Ensure assembler source is preprocessed
set_source_files_properties(first_arm_function.asm PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")

# Keep assembler compile flag generically
target_compile_options(img_to_ascii PRIVATE
        $<$<COMPILE_LANGUAGE:ASM>:-x assembler-with-cpp>
)