CC ?= cc
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS ?= -std=c11 -O2 -g
CFLAGS += -Wall -Wextra -Werror
LDLIBS += -pthread
BUILD_DIR ?= build
BIN_DIR ?= bin

CORE_SRC := src/fs/alloc.c src/fs/icache.c src/fs/inode.c src/fs/file.c \
            src/fs/dir.c src/fs/path.c src/fs/fs.c src/vdev/disk.c \
            src/sys/syscall.c src/user/user.c
CMD_SRC := $(wildcard src/cmd/*.c)
CORE_OBJ := $(CORE_SRC:%.c=$(BUILD_DIR)/%.o)
CMD_OBJ := $(CMD_SRC:%.c=$(BUILD_DIR)/%.o)
SHELL_OBJ := $(BUILD_DIR)/src/shell/sh.o
MKFS_OBJ := $(BUILD_DIR)/tools/mkfs.o
TEST_OBJ := $(BUILD_DIR)/tests/test_fs.o $(BUILD_DIR)/tests/test_io.o
DEPS := $(CORE_OBJ:.o=.d) $(CMD_OBJ:.o=.d) $(SHELL_OBJ:.o=.d) \
        $(MKFS_OBJ:.o=.d) $(TEST_OBJ:.o=.d)

all: $(BIN_DIR)/mkfs $(BIN_DIR)/sh
mkfs: $(BIN_DIR)/mkfs
sh: $(BIN_DIR)/sh

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BIN_DIR)/mkfs: $(MKFS_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BIN_DIR)/sh: $(CORE_OBJ) $(CMD_OBJ) $(SHELL_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BIN_DIR)/test_fs: $(CORE_OBJ) $(BUILD_DIR)/tests/test_fs.o
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BIN_DIR)/test_io: $(CORE_OBJ) $(BUILD_DIR)/tests/test_io.o
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test: all $(BIN_DIR)/test_fs $(BIN_DIR)/test_io
	python3 tests/run_tests.py $(BIN_DIR)

user_test: test

sanitize:
	$(MAKE) BUILD_DIR=build/sanitize BIN_DIR=bin/sanitize \
	    CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Werror -fno-omit-frame-pointer -fsanitize=address,undefined' \
	    LDFLAGS='-fsanitize=address,undefined' test

clean:
	rm -rf build bin

-include $(DEPS)
.PHONY: all mkfs sh test user_test sanitize clean
