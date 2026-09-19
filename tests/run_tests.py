#!/usr/bin/env python3
"""Run API and shell checks against disposable filesystem images."""

import subprocess
import sys
import tempfile
from pathlib import Path


def run(args, *, cwd, commands=None, exit_code=0, timeout=60):
    data = commands.encode() if commands is not None else None
    try:
        result = subprocess.run(
            [str(arg) for arg in args],
            input=data,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=cwd,
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        output = (error.stdout or b"") + (error.stderr or b"")
        raise AssertionError(
            f"Timed out after {timeout}s: {args}\n"
            + output.decode(errors="replace")
        ) from error
    if result.returncode != exit_code:
        raise AssertionError(
            f"Expected exit {exit_code}, got {result.returncode}: {args}\n"
            f"stdout:\n{result.stdout.decode(errors='replace')}\n"
            f"stderr:\n{result.stderr.decode(errors='replace')}"
        )
    if any(marker in result.stderr for marker in
           (b"AddressSanitizer", b"LeakSanitizer", b"runtime error:")):
        raise AssertionError(
            f"Sanitizer diagnostic: {args}\n"
            + result.stderr.decode(errors="replace")
        )
    return result.stdout


def require(condition, message, output=b""):
    if not condition:
        raise AssertionError(message + "\n" + output.decode(errors="replace"))


def main():
    if len(sys.argv) != 2:
        raise SystemExit(f"Usage: {sys.argv[0]} BIN_DIR")
    binaries = Path(sys.argv[1]).resolve()
    mkfs = binaries / "mkfs"
    shell = binaries / "sh"
    with tempfile.TemporaryDirectory(prefix="minivfs-tests-") as temporary:
        cwd = Path(temporary)
        image = cwd / "api.img"
        run([mkfs, image, "4096"], cwd=cwd)
        run([binaries / "test_fs", image], cwd=cwd)
        run([binaries / "test_io", image], cwd=cwd)
        output = run([shell, image], cwd=cwd,
                     commands="cat /binary-output\n" * 140 + "rm /binary-output\nexit\n")
        require(output == bytes(i % 251 for i in range(4096)) * 140,
                "Repeated cat lost binary bytes or leaked descriptors")

        for blocks in ("0", "4294967295", "-1"):
            invalid = cwd / f"invalid-{blocks}.img"
            run([mkfs, invalid, blocks], cwd=cwd, exit_code=1)
            require(not invalid.exists(), "Rejected size created an image")
        run([shell, cwd / "missing.img"], cwd=cwd, commands="exit\n", exit_code=1)
        require(not (cwd / "missing.img").exists(), "Mount created a missing image")
        run([shell], cwd=cwd, exit_code=1)

        image = cwd / "shell.img"
        run([mkfs, image, "4096"], cwd=cwd)
        output = run([shell, image], cwd=cwd, commands=(
            "mkdir /docs\n"
            "echo this is longer than replacement > /docs/source\n"
            "echo old target tail must disappear > /docs/copy\n"
            "cp /docs/source /docs/copy\n"
            "echo short > /docs/source\n"
            "cp /docs/source /docs/copy\n"
            "cat /docs/copy\n"
            "ls /docs\n"
            "touch /docs/empty\n"
            "rm /docs/empty\n"
            "mkdir /remove\n"
            "rmdir /remove\n"
            "sync\n"
            "exit\n"
        ))
        require(output.startswith(b"short\n"), "Copy retained stale bytes", output)
        require(b"source" in output and b"copy" in output, "ls omitted files", output)
        output = run([shell, image], cwd=cwd, commands=(
            "cat /docs/source\ncat /docs/copy\nexit\n"
        ))
        require(output == b"short\nshort\n", "File contents did not persist", output)

        run([shell, image], cwd=cwd, commands="cp /docs/source /docs/./source\nexit\n",
            exit_code=1)
        output = run([shell, image], cwd=cwd, commands="cat /docs/source\nexit\n")
        require(output == b"short\n", "Self-copy changed its source", output)

        output = run([shell, image], cwd=cwd, commands=(
            "usertest 1\n"
            "atomtest\natomtest\n"
            "stressfs 1\nstressfs 1\n"
            "sync\nexit\n"
        ))
        require(output.count(b"[UserTest] PASS:") == 1,
                "usertest did not report success", output)
        require(output.count(b"[AtomTest] PASS:") == 2,
                "atomtest did not pass twice", output)
        require(output.count(b"[StressFS] PASS:") == 2,
                "stressfs did not pass twice", output)

        output = run([shell, image], cwd=cwd, commands="ls /\nexit\n")
        require(all(name not in output for name in
                    (b"usertest.tmp", b"atomtest.tmp", b"stressfs.tmp")),
                "Test commands left temporary directories", output)
        run([shell, image], cwd=cwd, commands=(
            "rm /docs/source /docs/copy\nrmdir /docs\nsync\nexit\n"
        ))

    print("PASS: API, device errors, shell persistence, and repeated concurrency checks")


if __name__ == "__main__":
    main()
