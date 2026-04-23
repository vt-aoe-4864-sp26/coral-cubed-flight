board_runner_args(openocd --target-file "target/stm32l4x.cfg")
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)