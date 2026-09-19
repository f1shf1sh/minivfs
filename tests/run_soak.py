#!/usr/bin/env python3
"""Run a bounded soak test in one process, keeping its image and evidence."""

import argparse
import datetime
import json
import math
import os
from pathlib import Path
import platform
import shutil
import signal
import subprocess
import sys
import time


def utc_now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def sample_process(pid, vcgencmd):
    sample = {"utc": utc_now(), "pid": pid}
    proc = Path("/proc") / str(pid)
    try:
        fields = dict(line.split(":", 1) for line in (proc / "status").read_text().splitlines())
        for key in ("VmRSS", "VmHWM", "VmSize", "Threads"):
            if key in fields:
                sample[key] = int(fields[key].split()[0])
        sample["fd_count"] = len(list((proc / "fd").iterdir()))
    except (OSError, ValueError):
        pass  # /proc is optional, and the process can exit during sampling.
    if vcgencmd:
        for key in ("measure_temp", "get_throttled"):
            try:
                result = subprocess.run([vcgencmd, key], capture_output=True, text=True, timeout=3)
                sample[key] = result.stdout.strip() if result.returncode == 0 else None
            except (OSError, subprocess.TimeoutExpired):
                sample[key] = None
    return sample


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    duration = parser.add_mutually_exclusive_group()
    duration.add_argument("--hours", type=float, help="duration in hours, up to 48 (default: 24)")
    duration.add_argument("--seconds", type=int, help="short smoke-test duration")
    parser.add_argument("--threads", type=int, default=4)
    parser.add_argument("--epoch-seconds", type=int, default=60,
                        help="work period before sync/remount/checkpoint, 1..60")
    parser.add_argument("--bin-dir", type=Path, default=Path(__file__).resolve().parents[1] / "bin")
    parser.add_argument("--run-dir", type=Path, help="new directory for image, logs and summary")
    parser.add_argument("--sample-seconds", type=int, default=30)
    parser.add_argument("--stall-seconds", type=int, default=300,
                        help="maximum time without test output")
    args = parser.parse_args()
    hours = args.hours if args.hours is not None else 24
    if args.seconds is None and (not math.isfinite(hours) or not 0 < hours <= 48):
        parser.error("--hours must be greater than zero and at most 48")
    args.seconds = args.seconds if args.seconds is not None else math.ceil(hours * 3600)
    if not 1 <= args.seconds <= 172800:
        parser.error("duration must be 1..172800 seconds")
    if not 1 <= args.threads <= 8 or not 1 <= args.epoch_seconds <= 60:
        parser.error("threads must be 1..8 and epoch-seconds must be 1..60")
    if args.sample_seconds < 1 or args.stall_seconds <= args.epoch_seconds:
        parser.error("sample-seconds must be positive; stall-seconds must exceed epoch-seconds")
    args.bin_dir = args.bin_dir.resolve()
    for name in ("mkfs", "soak"):
        if not os.access(args.bin_dir / name, os.X_OK):
            parser.error(f"missing executable {args.bin_dir / name}; run make soak first")
    if args.run_dir is None:
        stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        args.run_dir = Path(__file__).resolve().parents[1] / "soak-results" / stamp
    args.run_dir = args.run_dir.resolve()
    return args


def main():
    args = arguments()
    try:
        args.run_dir.mkdir(parents=True, exist_ok=False)
    except FileExistsError:
        raise SystemExit(f"Refusing existing run directory: {args.run_dir}")
    image = args.run_dir / "disk.img"
    log_path = args.run_dir / "soak.log"
    summary_path = args.run_dir / "summary.json"
    metrics_path = args.run_dir / "metrics.jsonl"
    command = [str(args.bin_dir / "soak"), str(image), str(args.seconds),
               str(args.threads), str(args.epoch_seconds)]
    summary = {"status": "RUNNING", "started_utc": utc_now(),
               "requested_seconds": args.seconds, "threads": args.threads,
               "epoch_seconds": args.epoch_seconds, "command": command,
               "platform": platform.platform(), "machine": platform.machine(),
               "image": str(image), "log": str(log_path), "metrics": str(metrics_path)}
    summary_path.write_text(json.dumps(summary, indent=2) + "\n")
    print(f"Run directory: {args.run_dir}", flush=True)
    requested_signal = 0

    def request_stop(number, _frame):
        nonlocal requested_signal
        requested_signal = number

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)
    process = None
    samples = []
    start = None
    status = None
    error = None
    try:
        formatted = subprocess.run([str(args.bin_dir / "mkfs"), str(image)],
                                   capture_output=True, text=True, timeout=60)
        (args.run_dir / "mkfs.log").write_text(formatted.stdout + formatted.stderr)
        if formatted.returncode:
            raise RuntimeError("mkfs failed; see mkfs.log")
        if requested_signal:
            status = "INTERRUPTED"
        else:
            vcgencmd = shutil.which("vcgencmd")
            with log_path.open("wb") as output, metrics_path.open("w") as metrics:
                process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=output,
                                           stderr=subprocess.STDOUT, start_new_session=True)
                summary["pid"] = process.pid
                summary_path.write_text(json.dumps(summary, indent=2) + "\n")
                start = time.monotonic()
                next_sample = start
                last_output = start
                position = 0
                grace_deadline = None
                with log_path.open("rb") as reader:
                    while process.poll() is None:
                        now = time.monotonic()
                        chunk = reader.read()
                        if chunk:
                            position += len(chunk)
                            last_output = now
                            print(chunk.decode(errors="replace"), end="", flush=True)
                        if now >= next_sample:
                            sample = sample_process(process.pid, vcgencmd)
                            sample["elapsed_seconds"] = round(now - start, 3)
                            metrics.write(json.dumps(sample) + "\n")
                            metrics.flush()
                            samples.append(sample)
                            next_sample = now + args.sample_seconds
                        now = time.monotonic()
                        if status is None:
                            if requested_signal:
                                status = "INTERRUPTED"
                            elif now - last_output > args.stall_seconds:
                                status = "TIMEOUT"
                                error = "No test progress within stall timeout"
                            elif now - start > args.seconds + args.stall_seconds:
                                status = "TIMEOUT"
                                error = "Test exceeded duration plus shutdown allowance"
                            if status:
                                process.send_signal(signal.SIGTERM)
                                grace_deadline = now + 30
                        if grace_deadline is not None and now >= grace_deadline:
                            process.kill()
                            process.wait()
                            break
                        time.sleep(0.2)
                    remainder = reader.read()
                    position += len(remainder)
                    if remainder:
                        print(remainder.decode(errors="replace"), end="", flush=True)
                summary["log_bytes"] = position
            summary["returncode"] = process.wait()
            summary["elapsed_seconds"] = round(time.monotonic() - start, 3)
            text = log_path.read_text(errors="replace")
            if status is None:
                diagnostics = any(item in text for item in
                                  ("AddressSanitizer", "LeakSanitizer", "runtime error:"))
                if requested_signal:
                    status = "INTERRUPTED"
                elif summary["elapsed_seconds"] > args.seconds + args.stall_seconds:
                    status = "TIMEOUT"
                    error = "Test exceeded duration plus shutdown allowance"
                elif (process.returncode == 0 and "SOAK PASS" in text and not diagnostics
                        and summary["elapsed_seconds"] >= args.seconds):
                    status = "PASS"
                elif process.returncode == 130:
                    status = "INTERRUPTED"
                else:
                    status = "FAIL"
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as exception:
        error = str(exception)
        status = "FAIL"
    finally:
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        summary["status"] = status or "FAIL"
        summary["finished_utc"] = utc_now()
        if error:
            summary["error"] = error
        for field in ("VmRSS", "VmHWM", "fd_count"):
            values = [sample[field] for sample in samples if field in sample]
            if values:
                summary[field] = {"first": values[0], "last": values[-1],
                                  "minimum": min(values), "maximum": max(values)}
        summary_path.write_text(json.dumps(summary, indent=2) + "\n")
    print(f"SOAK RESULT: {summary['status']}; summary: {summary_path}", flush=True)
    return 0 if summary["status"] == "PASS" else 130 if summary["status"] == "INTERRUPTED" else 1


if __name__ == "__main__":
    sys.exit(main())
