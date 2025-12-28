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

set(PROJECT_SOURCES
        src/main.cpp
        src/image_loader.cpp
        src/image_converter.cpp
)

# Always include the assembly implementation in the build so the binary
# contains the NEON/ASM versions. Runtime flag `--asm-on/--asm-off`
# controls whether the program uses ASM or the C++ implementations.
list(APPEND PROJECT_SOURCES first_arm_function.asm)
add_compile_definitions(BUILD_WITH_ASM)

add_executable(
        img_to_ascii
        ${PROJECT_SOURCES}
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

# Assembler optimization flags
target_compile_options(img_to_ascii PRIVATE -O3 -march=armv8-a)
# Ensure assembler source is preprocessed
set_source_files_properties(first_arm_function.asm PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")