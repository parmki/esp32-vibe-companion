#!/usr/bin/env python3
"""
Capture the boot banner from the board.

Toggles DTR/RTS to force a hardware reset first, so the banner is captured even
though the upload just reset the chip a few seconds ago.

    sg dialout -c "python3 scripts/capture_serial.py --seconds 20"
"""

import argparse
import subprocess
import sys
import time

PORT = "/dev/ttyUSB0"
BAUD = 115200


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default=PORT)
    ap.add_argument("--baud", type=int, default=BAUD)
    ap.add_argument("--seconds", type=float, default=20.0)
    args = ap.parse_args()

    # fail fast and clearly if we cannot open the port
    try:
        import serial
    except ImportError:
        sys.exit("pyserial missing:  python3 -m pip install --user pyserial")

    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
    except PermissionError:
        sys.exit(f"permission denied on {args.port} -- run this via:  "
                 f"sg dialout -c \"python3 {__file__}\"")

    # hardware reset: pull RTS low briefly (ESP32 auto-reset circuit)
    ser.setDTR(False)
    ser.setRTS(True)
    time.sleep(0.12)
    ser.setRTS(False)
    time.sleep(0.12)
    ser.reset_input_buffer()

    deadline = time.time() + args.seconds
    got = 0
    while time.time() < deadline:
        raw = ser.readline()
        if raw:
            got += 1
            print(raw.decode("utf-8", "replace").rstrip())
    ser.close()
    if got == 0:
        print(f"(no serial output in {args.seconds}s)", file=sys.stderr)


if __name__ == "__main__":
    main()
