# 1. Intercept the default make command by placing this BEFORE any includes.
# This ensures that when you run 'make', it runs our custom move target.
default_target: move_artifacts

PROJECT = com
BUILD_DIR = build

SUPPORT_DIR = ./libs
CFILES = com_main.c
# include support sources explicitly so they get compiled
CFILES += com.c tab.c

# Edit these two lines as needed
DEVICE=nrf52833qdaa
OOCD_FILE = board/nrf52833-generic.cfg

# All lines below probably should not be edited
VPATH += $(SUPPORT_DIR)
INCLUDES += $(patsubst %,-I%, . $(SUPPORT_DIR))
OPENCM3_DIR=../../../third-party/libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include ../../../make/rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk

# 2. Define the post-build sequence AFTER the includes.
# This depends on the standard 'all' target from rules.mk completing first.
move_artifacts: all
	@echo "Moving build artifacts to $(BUILD_DIR)/..."
	@mkdir -p $(BUILD_DIR)
	@mv -f $(PROJECT).elf $(PROJECT).bin $(PROJECT).hex $(PROJECT).map *.ld $(BUILD_DIR)/ 2>/dev/null || true