#!/usr/bin/env python3
"""Generate a Musically Unorthodox Pro key for a buyer's email.

Usage:  python3 make_key.py buyer@example.com
Keep SECRET identical to kSecret in Source/license/License.cpp.
"""
import hashlib, sys

SECRET = "musically-unorthodox-2026-mix-assistant"

def make_key(email: str) -> str:
    email = email.strip().lower()
    hexdigest = hashlib.sha256(f"{email}|{SECRET}".encode("utf-8")).hexdigest().upper()
    return "MU-" + "-".join(hexdigest[i * 4:i * 4 + 4] for i in range(4))

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)
    print(make_key(sys.argv[1]))
