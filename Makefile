AR=$(CROSS_COMPILE)ar
CC=$(CROSS_COMPILE)gcc
CFLAGS=-I./include/ -I./src/qnx/ -DSDL_ENABLE_OLD_NAMES -DSDL_JOYSTICK_DINPUT
LDFLAGS=-L./build/

OUT_DIR=build
OBJ_DIR=$(OUT_DIR)/obj

OUT_LIB_NAME=SDL3
OUT_LIB=$(OUT_DIR)/lib$(OUT_LIB_NAME).a

_LIB_OBJ=src/log.o src/qnx/hid.o src/sdl_glue.o src/event_queue.o \
	src/SDL_mouse.o src/SDL_keyboard.o src/SDL_joystick.o \
	src/SDL_guid.o src/SDL_sysjoystick.o src/SDL_gamepad.o
LIB_OBJ=$(_LIB_OBJ:%=$(OBJ_DIR)/%)

OBJDIRS:=$(sort $(patsubst %, $(OBJ_DIR)/%, $(dir $(_LIB_OBJ))))

all: test

$(OBJDIRS):
	mkdir -p $@

$(LIB_OBJ): | $(OBJDIRS)

$(OBJ_DIR)/%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(OUT_LIB): $(LIB_OBJ)
	$(AR) rcs -o $@ $^

library: $(OUT_LIB)

OUT_BIN=$(OUT_DIR)/s3input_test
_BIN_OBJ=src/gamepad_test.o
BIN_OBJ=$(_BIN_OBJ:%=$(OBJ_DIR)/%)
LIB_DEPS=-lhiddi -lm -l$(OUT_LIB_NAME)

$(BIN_OBJ): | $(OBJDIRS)

$(OUT_BIN): $(BIN_OBJ) $(OUT_LIB)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LIB_DEPS)

test: $(OUT_BIN)

.PHONY: clean library tests

clean:
	rm -rf $(OUT_DIR)
