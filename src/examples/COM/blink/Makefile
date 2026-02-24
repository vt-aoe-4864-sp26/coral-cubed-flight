PROJECT = blink
BUILD_DIR = build

#SUPPORT_DIR = ./support
CFILES = blink.c
CFILES += # SUPPORT_DIR C files

# Edit these two lines as needed
DEVICE=nrf52833qdaa
OOCD_FILE = board/nrf52833-generic.cfg

# All lines below probably should not be edited
VPATH += $(SUPPORT_DIR)
INCLUDES += $(patsubst %,-I%, . $(SUPPORT_DIR))
OPENCM3_DIR=../libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include ../rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk
