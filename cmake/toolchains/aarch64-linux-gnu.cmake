# Toolchain for RK3326 (Cortex-A35 / aarch64 Linux) with clang-21 and debian sysroot
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_SYSROOT /opt/rk3326-sysroot)

set(CMAKE_C_COMPILER /usr/bin/clang-21)
set(CMAKE_CXX_COMPILER /usr/bin/clang++-21)

set(TARGET_FLAGS "--target=aarch64-linux-gnu --gcc-toolchain=/usr -mcpu=cortex-a35 -fsigned-char -fno-math-errno -pipe")

set(CMAKE_C_FLAGS_INIT "${TARGET_FLAGS} -O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_INIT "${TARGET_FLAGS} -O3 -DNDEBUG")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${TARGET_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${TARGET_FLAGS}")

set(CMAKE_FIND_ROOT_PATH /opt/rk3326-sysroot)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# PortMaster RPATH
set(CMAKE_BUILD_RPATH "$ORIGIN:$ORIGIN/libs.aarch64")
set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/libs.aarch64")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

set(Rust_CARGO_TARGET "aarch64-unknown-linux-gnu" CACHE STRING "")
