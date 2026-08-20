# Shared flashing targets for the STM32F722ZE (NUCLEO-F722ZE board).
#
#   ninja flash  - probe-rs download over SWD. There are usually several
#                  ST-Links on the PC (e.g. the disco's), so identify the
#                  nucleo's probe (VID:PID[:Serial]) with `probe-rs list` and
#                  pass -DDEBUG_PROBE=<selector>.
#
# Overrides:
#   -DPROBE_RS=/path/to/probe-rs   -DDEBUG_PROBE=<probe selector>
#   -DPROBE_RS_CHIP=STM32F722ZE    (default)

set(DEBUG_PROBE "" CACHE STRING
    "probe-rs --probe selector (VID:PID[:Serial]); empty = auto-detect")

if(DEBUG_PROBE)
    set(PROBE_ARGS --probe "${DEBUG_PROBE}")
else()
    set(PROBE_ARGS)
endif()

set(PROBE_RS_CHIP "STM32F722ZE" CACHE STRING "probe-rs target chip name")

find_program(PROBE_RS NAMES probe-rs probe-rs.exe
    HINTS "$ENV{USERPROFILE}/.cargo/bin" "$ENV{CARGO_HOME}/bin"
    DOC "probe-rs binary (preferred SWD flasher)")

set(BIN_HEX "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex")

if(PROBE_RS)
    add_custom_target(flash
        COMMAND "${PROBE_RS}" download ${PROBE_ARGS}
                    --chip "${PROBE_RS_CHIP}" --protocol swd
                    --binary-format hex --verify --reset --non-interactive
                    --disable-progressbars "${BIN_HEX}"
        DEPENDS hex
        COMMENT "Flashing ${PROJECT_NAME}.hex to ${PROBE_RS_CHIP} via probe-rs (${DEBUG_PROBE}) ..."
        USES_TERMINAL)
else()
    add_custom_target(flash
        COMMAND ${CMAKE_COMMAND} -E echo "probe-rs not found. Install it (cargo install probe-rs-tools) or pass -DPROBE_RS=/path/to/probe-rs.")
endif()