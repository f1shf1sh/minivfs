# MiniVFS

MiniVFS is a lightweight virtual file system implemented in C, supporting basic file operations and multi-threaded stress testing. It is designed for educational and experimental purposes, demonstrating file system concepts, caching, and concurrency control.

---

## Features

- Basic file system operations:
  - `ls`, `cat`, `rm`, `cp`, `mkdir`, `touch`, `echo`
- Supports **inode caching** and block caching
- Multi-threaded file operations with **file-level locking**
- Stress testing tool (`stressfs`) to evaluate system performance under concurrent operations
- Observes CPU, memory, and disk usage

---

## File System API

The project exposes a set of file system APIs for user-level commands:

| Function | Description |
|----------|-------------|
| `my_open` | Open or create a file |
| `my_close` | Close a file descriptor |
| `my_read` | Read from a file |
| `my_write` | Write to a file |
| `cmd_ls` | List directory contents |
| `cmd_cat` | Print file contents |
| `cmd_rm` | Remove a file |
| `cmd_cp` | Copy a file |
| `cmd_stressfs` | Run multi-threaded stress test |

> **Note:** APIs provide atomic operations, but thread safety is handled at the command layer using file locks.

---

## StressFS

