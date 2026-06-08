# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app)

target_sources(app PRIVATE
	src/main.c
	../../custom_drivers/mpu6050/mpu6050.c
)

include_directories(../../.)
