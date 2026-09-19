# MiniVFS

MiniVFS is a teaching filesystem written in C. It stores files and directories in
one disk-image file, mounted at `/` inside its own shell. It is a userspace
filesystem experiment, not a host operating-system mount or a complete POSIX VFS.

## Build and run

Requirements: a C11 compiler, GNU Make, POSIX file/thread APIs, and Python 3 for
the test runner.

```sh
make
./start.sh
```

`start.sh` builds the project and opens `disk.img` beside the script. It creates
and formats that image only if it does not exist. An optional image path is
resolved from the caller's working directory:

```sh
./start.sh /path/to/demo.img
./bin/sh /path/to/existing.img
```

For explicit formatting:

```sh
./bin/mkfs demo.img 4096
./bin/sh demo.img
```

**`mkfs` overwrites the named image.** Its optional size is a block count:
169–65,702 blocks, with 4,096 bytes per block. The default is 65,536 blocks
(256 MiB); the example creates a 16 MiB image.

The shell requires a valid existing image and does not format one itself.
Old images whose superblock has `block_size = 0` are rejected. Neither the shell
nor the startup script migrates or deletes an incompatible existing image;
preserve it and choose a new path for a freshly formatted image.

## Shell commands

```text
mkdir /notes
echo hello filesystem > /notes/message
cp /notes/message /notes/copy
cat /notes/copy
ls /notes
sync
rm /notes/message /notes/copy
rmdir /notes
exit
```

| Command | Behavior |
| --- | --- |
| `ls [path]` | List a directory, or show one file; default path is `.`. |
| `cat <file>` | Write the file's bytes to standard output. |
| `echo [words ...] [> file]` | Print words and a newline; `>` replaces the destination contents. |
| `cp <source> <dest>` | Copy a regular file, replacing the destination; reject self-copy. |
| `touch <file> [file ...]` | Create missing files without truncating existing files. |
| `rm <file> [file ...]` | Unlink regular files. |
| `mkdir <directory> [directory ...]` | Create directories; parents must already exist. |
| `rmdir <directory> [directory ...]` | Remove empty directories. |
| `fdisk` | Show image capacity, free blocks/inodes, and the file-size limit. |
| `sync`, `exit` | Flush changes, or flush and unmount before exiting. |
| `usertest [size_in_MiB]` | Write and compare deterministic bytes; default 1 MiB. |
| `atomtest` | Verify complete, unique append records from four threads. |
| `stressfs [seconds]` | Four threads create, write, read, and unlink separate files; default 1 second, range 1–60. |

The shell splits input on whitespace; it has no quoting, pipes, globbing,
`cd`, or general redirection. Use spaces around `>`. Relative paths start at
the filesystem root; `.` and `..` are resolved as directory entries. Input is
limited to 1,024 bytes and 32 arguments. A failed command makes the shell's
eventual exit status nonzero.

The three demonstration tests use `/usertest.tmp`, `/atomtest.tmp`, and
`/stressfs.tmp`. Each refuses an existing entry at its reserved path,
checks exact byte content/length, and removes its own temporary files on normal
completion. They do not calculate hashes or measure host CPU/disk usage.
Ctrl-C requests shell exit; an active command finishes and joins its workers
before the filesystem is unmounted.

## API and limits

Applications include [`include/user.h`](include/user.h). The public `my_*` API
provides mount/unmount/sync, open/read/write/close, unlink/mkdir/rmdir,
stat/readdir/statfs. Failures return `-1` and set `errno`; read/write return byte
counts and may return a short count. Directory iteration returns 1 for an entry,
0 at the end, or -1 on failure, starting with an offset of zero.

Supported open flags are `O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREAT`,
`O_APPEND`, and `O_TRUNC`. Open descriptors remain usable after unlink until
their final close. Each public call holds one shared mutex, including append
offset selection and writing. Calls are thread-safe but run serially; an entire
multi-call command is not a transaction.

- One mounted image and one mount point, `/`, per process.
- 127 descriptors; a separate, fixed 64-slot inode cache limits simultaneously
  referenced inodes, including temporary references during path lookup.
- Maximum file size: **4,243,456 bytes** (12 direct blocks and one indirect block).
- Names: at most 59 bytes; paths: at most 4,095 bytes.
- Disk records: 4,096-byte blocks, 64-byte inodes, 64-byte directory entries,
  and an 88-byte superblock structure.

There is no crash recovery, sparse-file support, seek API, permissions model,
or supported multiple-backend/multiple-mount configuration. The reserved log
area is unused. Use `sync` or a normal exit to persist metadata; the format has
no cross-endian conversion. Concurrent access to one image by separate processes
is unsupported.

## Tests and implementation

```sh
make test
make sanitize
```

`make test` builds API/device tests and runs shell integration checks against
temporary images, including persistence and repeated concurrent demonstrations.
It does not use the project's `disk.img`. `make sanitize` runs the same suite
with AddressSanitizer and UndefinedBehaviorSanitizer in separate build/output
directories. `make clean` removes build outputs, not images.

The old
`src/vdev/cache.c` and `include/spinlock.h` are unconnected experimental drafts;
they are not built, and there is no active block cache.

## 24/48-hour soak tests

Build on the target machine, then run against a new, dedicated results directory:

```sh
make test
make soak
python3 tests/run_soak.py --hours 24 --run-dir ./soak-results/run-24h
# Use --hours 48 for a two-day run.
```

The runner keeps one test process alive, checks concurrent operations and cache
reuse, and verifies contents after periodic sync/unmount/remount cycles. It saves
the image, progress log, resource samples, and a final JSON result. Interruptions
and missing progress are recorded separately from success. It does not start a
background service or use the project's regular image.
