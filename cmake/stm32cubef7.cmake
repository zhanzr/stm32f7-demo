# Locate the shared STM32Cube_FW_F7 package that provides the F7 HAL + CMSIS
# (+ BSP/middleware) used by ALL boards in this repo. Override with
#   -DSTM32CUBE_F7=<root of an STM32Cube_FW_F7 install>
# The default is the STM32CubeMX repository copy of this workstation.
set(STM32CUBE_F7 "C:/Users/user1/STM32Cube/Repository/STM32Cube_FW_F7_V1.17.4" CACHE PATH
    "Root of the STM32Cube_FW_F7 package (shared by all boards)")

set(F7_HAL    ${STM32CUBE_F7}/Drivers/STM32F7xx_HAL_Driver)
set(F7_CMSIS  ${STM32CUBE_F7}/Drivers/CMSIS)
set(F7_CMSDEV ${F7_CMSIS}/Device/ST/STM32F7xx)
set(F7_BSP    ${STM32CUBE_F7}/Drivers/BSP)