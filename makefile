CC := gcc
CFLAGS := -Wall -Wextra -g -Iinclude
LDFLAGS := -pthread

# 目录定义
SRC_DIR := src
FS_DIR := src/fs
VDEV_DIR := src/vdev
SYS_DIR := src/sys
USER_DIR := src/user
TOOLS_DIR := tools
SH_DIR := src/shell
TEST_DIR := tests
BIN_DIR := bin
CMD_DIR := src/cmd

# 通用对象文件（不包含 shell 和测试）
COMMON_OBJ := $(FS_DIR)/dir.o $(FS_DIR)/fs.o $(FS_DIR)/inode.o $(FS_DIR)/path.o \
              $(SYS_DIR)/syscall.o $(USER_DIR)/user.o $(VDEV_DIR)/disk.o \
# 			  $(VDEV_DIR)/cache.o

# cmd对象
CMD_OBJ := $(CMD_DIR)/cmd_ls.o \
		   $(CMD_DIR)/cmd_cat.o \
		   $(CMD_DIR)/cmd_usertest.o \
		   $(CMD_DIR)/cmd_rm.o \
		   $(CMD_DIR)/cmd_echo.o \
		   $(CMD_DIR)/cmd_cp.o \
		   $(CMD_DIR)/cmd_stressfs.o \
		   $(CMD_DIR)/cmd_fdisk.o \
		   $(CMD_DIR)/cmd_atomtest.o \
		   $(CMD_DIR)/cmd_touch.o \
		   $(CMD_DIR)/cmd_table.o

# 工具对象
TOOLS_OBJ := $(TOOLS_DIR)/mkfs.o

# shell对象
SH_OBJ := $(SH_DIR)/sh.o

# 测试对象
TEST_OBJ := $(TEST_DIR)/user_test.o

all : mkfs sh
# 目录
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# 通用编译规则
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# mkfs 可执行文件
mkfs: $(COMMON_OBJ) $(TOOLS_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $^ $(LDFLAGS)

# user_test 可执行文件
user_test: $(COMMON_OBJ) $(TEST_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $^ $(LDFLAGS)

# shell 可执行文件
sh: $(COMMON_OBJ) $(SH_OBJ) $(CMD_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $^ $(LDFLAGS)

clean:
	rm -f $(FS_DIR)/*.o $(VDEV_DIR)/*.o $(SYS_DIR)/*.o $(USER_DIR)/*.o \
          $(TOOLS_DIR)/*.o $(SH_DIR)/*.o $(CMD_DIR)/*.o $(TEST_DIR)/*.o
	rm bin/sh bin/mkfs
.PHONY: all clean