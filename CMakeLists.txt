cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(test_project)

pico_sdk_init()

add_executable(test_ntsc_wall
        test_ntsc_wall.c
        )

# Pull in our (to be renamed) simple get you started dependencies
target_link_libraries(test_ntsc_wall pico_stdlib hardware_adc hardware_pwm)

# create map/bin/hex file etc.
pico_add_extra_outputs(test_ntsc_wall)

pico_enable_stdio_usb(test_ntsc_wall 0)

pico_enable_stdio_uart(test_ntsc_wall 0)
