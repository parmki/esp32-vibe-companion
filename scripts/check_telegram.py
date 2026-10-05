#!/usr/bin/env python3
"""
Verify the Telegram bot token in config.h actually resolves to a bot.

Reads the token from config.h and calls getMe. Only getMe -- never getUpdates,
because getUpdates would consume and advance the update queue that the ESP32
itself is polling, silently stealing the user's messages.

Prints the API result and the token's shape, never the token itself.

    python3 scripts/check_telegram.py
"""

import json
import os
import re
import sys
import urllib.error
import urllib.request


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    cfg = os.path.abspath(os.path.join(here, "..", "config.h"))

    if not os.path.exists(cfg):
        sys.exit(f"config.h not found at {cfg}")

    m = re.search(r'#define\s+BOT_TOKEN\s+"(.*?)"', open(cfg).read())
    if not m:
        sys.exit("config.h has no BOT_TOKEN define")
    token = m.group(1)

    shape = f"{len(token)} chars, colon at index {token.find(':')}"
    print(f"token shape: {shape}")

    if not re.fullmatch(r"\d{6,}:[A-Za-z0-9_-]{30,}", token):
        print("VERDICT: malformed -- not <digits>:<35ish chars>", file=sys.stderr)
        return 2

    url = f"https://api.telegram.org/bot{token}/getMe"
    try:
        with urllib.request.urlopen(url, timeout=20) as r:
            body = json.load(r)
    except urllib.error.HTTPError as e:
        body = json.load(e)
    except Exception as e:
        sys.exit(f"network error: {type(e).__name__}: {e}")

    print(json.dumps(body, indent=2))
    if body.get("ok"):
        bot = body["result"]
        print(f"\nVERDICT: token is VALID -- bot @{bot.get('username')} "
              f"(id {bot.get('id')})")
        return 0
    print(f"\nVERDICT: token is NOT usable ({body.get('error_code')} "
          f"{body.get('description')}) -- get a fresh one from @BotFather",
          file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
