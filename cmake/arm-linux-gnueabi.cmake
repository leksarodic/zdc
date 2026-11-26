# Cross-compile for the Allwinner F1C200s / F1C500 (ARM926EJ-S, ARMv5TE, soft-float).
# This produces a Linux framebuffer/evdev binary for a Linux port of the 7010B.
# It is NOT a replacement for the stock Melis/ePDK app.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-linux-gnueabi-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabi-g++)
set(CMAKE_ASM_COMPILER arm-linux-gnueabi-gcc)

set(CMAKE_C_FLAGS_INIT "-march=armv5te -mtune=arm926ej-s -mfloat-abi=soft")
set(CMAKE_CXX_FLAGS_INIT "-march=armv5te -mtune=arm926ej-s -mfloat-abi=soft")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-march=armv5te -mfloat-abi=soft")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
