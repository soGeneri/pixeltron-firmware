# Host build of the Pixeltron component: unit tests and the shared library
# the desktop simulator (Pixeltron repo, sim/virtual_device.py) loads.
#
#   make test     build and run the unit tests
#   make lib      build libpixeltron.dylib / .so for the simulator

ROOT := ..
SRCS := $(ROOT)/src/gesture.c $(ROOT)/src/brightness.c $(ROOT)/src/fb.c \
        $(ROOT)/src/screens.c $(ROOT)/src/codeword.c \
        $(ROOT)/third_party/qrcodegen/qrcodegen.c
CFLAGS := -std=c11 -Wall -Wextra -O2 -I$(ROOT)/include -I$(ROOT)/third_party/qrcodegen
BUILD := build

ifeq ($(shell uname),Darwin)
LIB := $(BUILD)/libpixeltron.dylib
LIBFLAGS := -dynamiclib
else
LIB := $(BUILD)/libpixeltron.so
LIBFLAGS := -shared -fPIC
endif

.PHONY: all test lib clean
all: test lib

lib: $(LIB)

$(LIB): $(SRCS) $(wildcard $(ROOT)/include/*.h $(ROOT)/src/*.h)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -fPIC $(LIBFLAGS) -o $@ $(SRCS)

$(BUILD)/test_pixeltron: test_pixeltron.c $(SRCS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_pixeltron.c $(SRCS)

test: $(BUILD)/test_pixeltron
	./$(BUILD)/test_pixeltron

clean:
	rm -rf $(BUILD)
