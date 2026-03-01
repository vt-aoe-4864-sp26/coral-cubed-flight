PROJECT = blink
BUILD_DIR = build

#SUPPORT_DIR = ./support
CFILES = blink.c
CFILES += # SUPPORT_DIR C files

# Edit these two lines as needed
DEVICE = stm32l496rgt3
OOCD_FILE = board/stm32l4-generic.cfg

# All lines below probably should not be edited
VPATH += $(SUPPORT_DIR)
INCLUDES += $(patsubst %,-I%, . $(SUPPORT_DIR))
OPENCM3_DIR=../libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include ../rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk
