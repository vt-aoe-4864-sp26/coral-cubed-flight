default_target: move_artifacts

PROJECT = cdh
BUILD_DIR = build

SUPPORT_DIR = ./libs
CFILES = cdh_main.c
# include support sources explicitly so they get compiled
CFILES += cdh.c tab.c

# Edit these two lines as needed
DEVICE=stm32l496rgt3
OOCD_FILE = board/stm32l4-generic.cfg

# All lines below probably should not be edited
VPATH += $(SUPPORT_DIR)
INCLUDES += $(patsubst %,-I%, . $(SUPPORT_DIR))
OPENCM3_DIR=../../../third-party/libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include ../../../make/rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk

# Define the post-build sequence AFTER the includes.
# This depends on the standard 'all' target from rules.mk completing first.
move_artifacts: all
	@echo "Moving build artifacts to $(BUILD_DIR)/..."
	@mkdir -p $(BUILD_DIR)
	@mv -f $(PROJECT).elf $(PROJECT).bin $(PROJECT).hex $(PROJECT).map *.ld $(BUILD_DIR)/ 2>/dev/null || true