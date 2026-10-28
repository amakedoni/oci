CC ?= cc
PYTHON ?= python3
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic -Werror
BUILD ?= build
PROGRAMS := $(BUILD)/parent $(BUILD)/child

.PHONY: all clean test sanitize
all: $(PROGRAMS)

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/parent: $(BUILD)/parent.o $(BUILD)/common.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/child: $(BUILD)/child.o $(BUILD)/common.o $(BUILD)/division.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

test: all
	LAB2_PARENT='$(abspath $(BUILD)/parent)' $(PYTHON) tests/test_lab2.py

sanitize:
	$(MAKE) BUILD=build/sanitize CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' test

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d)
