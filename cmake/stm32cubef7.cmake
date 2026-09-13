# Locate the F7 HAL + CMSIS drivers used by the boards in this repo.
#
# Default: the VENDORED copy in the repo root `drivers/` folder (a trimmed
# STM32Cube_FW_F7 v1.17.4 `Drivers` subtree - see drivers/README.md), so the
# repo builds without any external package install.
#
# To use a full STM32Cube_FW_F7 package instead, point STM32F7_HAL_ROOT at its
# `Drivers` directory:
#   -DSTM32F7_HAL_ROOT="C:/Users/user1/STM32Cube/Repository/STM32Cube_FW_F7_V1.17.4/Drivers"
set(STM32F7_HAL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../drivers" CACHE PATH
    "Root of the STM32F7 HAL + CMSIS (vendored `drivers/` or a full STM32Cube_FW_F7 package's Drivers dir)")

set(F7_HAL    ${STM32F7_HAL_ROOT}/STM32F7xx_HAL_Driver)
set(F7_CMSIS  ${STM32F7_HAL_ROOT}/CMSIS)
set(F7_CMSDEV ${F7_CMSIS}/Device/ST/STM32F7xx)
set(F7_BSP    ${STM32F7_HAL_ROOT}/BSP)
