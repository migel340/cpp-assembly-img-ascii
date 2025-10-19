cmake_minimum_required(VERSION 3.15)
project(img_to_ascii LANGUAGES CXX C ASM)

set(CMAKE_CXX_STANDARD 23)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(
        img_to_ascii
        main.cpp
        first_arm_function.asm
)


#set_source_files_properties(first_arm_function.asm PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")

#target_compile_options(img_to_ascii PRIVATE
#        $<$<COMPILE_LANGUAGE:ASM>:-x assembler-with-cpp>
#)