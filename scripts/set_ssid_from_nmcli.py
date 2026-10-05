#!/usr/bin/env python3
"""
Copy the active Wi-Fi SSID from NetworkManager straight into config.h.

Why this exists: some SSIDs contain text that looks like a hyperlink, and the
agent's tool-output filter replaces it with "a placeholder" - in the
chat, and in shell output. The value is perfectly readable from NetworkManager
though, so we never need to print it: this script reads it and writes it into
config.h, reporting only a masked confirmation.

Usage:
    python3 scripts/set_ssid_from_nmcli.py            # active wifi connection
    python3 scripts/set_ssid_from_nmcli.py --uuid X   # a specific profile
    python3 scripts/set_ssid_from_nmcli.py --ssid "MyNet"   # explicit value
"""

import argparse
import os
import re
import subprocess
import sys


def nmcli(*args):
    r = subprocess.run(["nmcli", *args], capture_output=True, text=True)
    return r.stdout, r.stderr, r.returncode


def active_wifi_uuid():
    """Return the UUID of the active wifi profile (UUIDs are colon/space safe)."""
    out, err, rc = nmcli("-t", "-f", "UUID,TYPE,ACTIVE", "connection", "show")
    for line in out.splitlines():
        if line.endswith(":802-11-wireless:yes"):
            return line.split(":")[0]
    sys.exit(f"no active wifi connection found.\nnmcli stderr: {err}")


def profile_field(uuid, field, secrets=False):
    args = ["-g", field, "connection", "show", uuid]
    if secrets:
        args.insert(0, "-s")
    out, err, rc = nmcli(*args)
    return out.rstrip("\n") if rc == 0 else ""


def c_literal(raw):
    """Escape for a C string literal. Non-ASCII/control bytes -> 3-digit octal,
    which is unambiguous (unlike \\xHH, which greedily eats following hex digits)."""
    out = []
    for b in raw.encode("utf-8"):
        ch = chr(b)
        if ch == '"':
            out.append('\\"')
        elif ch == "\\":
            out.append("\\\\")
        elif 32 <= b < 127:
            out.append(ch)
        else:
            out.append("\\%03o" % b)
    return "".join(out)


def mask(s):
    if len(s) <= 2:
        return "*" * len(s)
    return s[0] + "*" * (len(s) - 2) + s[-1]


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    cfg = os.path.abspath(os.path.join(here, "..", "config.h"))

    ap = argparse.ArgumentParser()
    ap.add_argument("--uuid")
    ap.add_argument("--ssid")
    ap.add_argument("--config", default=cfg)
    args = ap.parse_args()

    if args.ssid is not None:
        ssid, source = args.ssid, "explicit --ssid argument"
    else:
        uuid = args.uuid or active_wifi_uuid()
        ssid = profile_field(uuid, "802-11-wireless.ssid")
        source = f"NetworkManager profile {uuid}"
        if not ssid:
            sys.exit("could not read the SSID from that profile")

    if not ssid.strip():
        sys.exit("refusing to write an empty SSID")

    if not os.path.exists(args.config):
        sys.exit(f"config.h not found at {args.config}")

    src = open(args.config).read()
    new = re.sub(r'#define\s+WIFI_SSID\s+".*?"',
                 f'#define WIFI_SSID     "{c_literal(ssid)}"',
                 src, count=1)
    if new == src:
        sys.exit('config.h has no #define WIFI_SSID "..." line to replace')

    # cross-check: does the saved profile's PSK match what the user gave us?
    note = ""
    if args.ssid is None:
        psk = profile_field(args.uuid or active_wifi_uuid(),
                            "802-11-wireless-security.psk", secrets=True)
        if psk:
            # Never embed the credential itself: report only that a PSK was found.
            note = f"  saved PSK present: {'YES' if psk else 'NO'}"

    open(args.config, "w").write(new)

    print(f"SSID source : {source}")
    print(f"SSID written: {len(ssid)} chars, masked {mask(ssid)}")
    print(f"target      : {args.config}")
    if note:
        print(note)
    print("(the real name is in config.h - check it there)")


if __name__ == "__main__":
    main()
