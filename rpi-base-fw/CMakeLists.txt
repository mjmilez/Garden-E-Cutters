cmake_minimum_required(VERSION 3.16)
project(base_fw_rpi C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(base-fw-rpi
	src/main.c
	src/base_led.c
)

target_link_libraries(base-fw-rpi
	pthread
	gpiod
)