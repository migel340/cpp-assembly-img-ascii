cmake_minimum_required(VERSION 3.15)
project(img_to_ascii LANGUAGES CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_language(ASM)

add_executable(
        img_to_ascii
        src/main.cpp
        first_arm_function.asm
        src/image_loader.cpp
)

# Dodaj katalog include do ścieżek nagłówków
target_include_directories(img_to_ascii PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Ensure assembler source is preprocessed
set_source_files_properties(first_arm_function.asm PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")

# Keep assembler compile flag generically
target_compile_options(img_to_ascii PRIVATE
        $<$<COMPILE_LANGUAGE:ASM>:-x assembler-with-cpp>
)