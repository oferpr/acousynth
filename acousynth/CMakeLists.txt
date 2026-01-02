# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
set(PICO_EXTRAS_PATH ${USERHOME}/Engineering/Sources/pico-extras-master)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")



# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)


project(acousynth C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(acousynth
    main.cpp
    input_config.cpp
    output_config.cpp
    analysis.cpp
    wavetables.cpp
    libs/kissfft/kiss_fft.c
    libs/kissfft/kiss_fftr.c
)
pico_set_program_name(acousynth "acousynth")
pico_set_program_version(acousynth "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(acousynth 0)
pico_enable_stdio_usb(acousynth 1)


# Add any requested libraries
target_link_libraries(acousynth
    pico_stdlib
    hardware_adc
    hardware_dma
    hardware_irq
    pico_audio_i2s
)
#tell the compiler to compile for the M0+ processor
add_compile_definitions(ARM_MATH_CM0PLUS)

# Add the standard include files to the build
target_include_directories(acousynth PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_SOURCE_DIR}/libs/kissfft
        )



pico_add_extra_outputs(acousynth)

