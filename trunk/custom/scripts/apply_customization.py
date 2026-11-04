#!/usr/bin/env python3
"""Apply validated router defaults without constructing sed commands from input."""

import ipaddress
import json
import os
import re
import sys
from pathlib import Path


ALLOWED_KEYS = {
    "lanip", "signaccount", "signpassword",
    "wifi2gssid", "wifi2gpsk", "wifi5gssid", "wifi5gpsk",
}


def fail(message):
    raise SystemExit("ERROR: " + message)


def c_string(value):
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def validate_text(key, value):
    if not isinstance(value, str):
        fail(f"{key} must be a string")
    if any(ord(ch) < 32 or ord(ch) == 127 for ch in value):
        fail(f"{key} contains control characters")


def replace_define(text, name, value):
    pattern = re.compile(r"(?m)^(#define\\s+" + re.escape(name) + r"\\s+).*$")
    updated, count = pattern.subn(lambda m: m.group(1) + c_string(value), text, count=1)
    if count != 1:
        fail(f"could not locate exactly one #define {name}")
    return updated


def main():
    if len(sys.argv) != 3:
        fail("usage: apply_customization.py DEFAULTS_H CN_DICT")

    data = json.loads(os.environ.get("CUSTOMIZATION_JSON", "{}"))
    if not isinstance(data, dict):
        fail("customization JSON must be an object")
    unknown = set(data) - ALLOWED_KEYS
    if unknown:
        fail("unknown customization keys: " + ", ".join(sorted(unknown)))

    for key, value in data.items():
        validate_text(key, value)

    macros = {}
    lanip = data.get("lanip", "")
    if lanip:
        try:
            addr = ipaddress.IPv4Address(lanip)
        except ipaddress.AddressValueError:
            fail("lanip must be a valid IPv4 address")
        octets = lanip.split(".")
        host = int(octets[3])
        if host < 1 or host > 254 or 100 <= host <= 244:
            fail("lanip host must be .1-.99 or .245-.254 (DHCP pool is .100-.244)")
        prefix = ".".join(octets[:3])
        macros["DEF_LAN_ADDR"] = lanip
        macros["DEF_LAN_DHCP_BEG"] = prefix + ".100"
        macros["DEF_LAN_DHCP_END"] = prefix + ".244"

    account = data.get("signaccount", "")
    if account:
        if len(account) > 32 or not re.fullmatch(r"[A-Za-z0-9_-]+", account):
            fail("signaccount must be 1-32 letters, digits, '_' or '-'")
        macros["SYS_USER_ROOT"] = account

    password = data.get("signpassword", "")
    if password:
        if len(password) > 128 or not password.isascii() or not password.isprintable():
            fail("signpassword must be at most 128 printable ASCII characters")
        macros["DEF_ROOT_PASSWORD"] = password

    for key, macro in (
        ("wifi2gssid", "DEF_WLAN_2G_SSID"),
        ("wifi5gssid", "DEF_WLAN_5G_SSID"),
    ):
        value = data.get(key, "")
        if value:
            if len(value.encode("utf-8")) > 32:
                fail(f"{key} must be at most 32 UTF-8 bytes")
            macros[macro] = value

    for key, macro in (
        ("wifi2gpsk", "DEF_WLAN_2G_PSK"),
        ("wifi5gpsk", "DEF_WLAN_5G_PSK"),
    ):
        value = data.get(key, "")
        if value:
            if not value.isascii() or not value.isprintable():
                fail(f"{key} must contain printable ASCII")
            if not (8 <= len(value) <= 63 or
                    (len(value) == 64 and re.fullmatch(r"[0-9A-Fa-f]{64}", value))):
                fail(f"{key} must be 8-63 printable characters or a 64-digit hex PSK")
            macros[macro] = value

    defaults_path = Path(sys.argv[1])
    dict_path = Path(sys.argv[2])
    defaults = defaults_path.read_text(encoding="utf-8")
    dictionary = dict_path.read_text(encoding="utf-8") if lanip else None

    for name, value in macros.items():
        defaults = replace_define(defaults, name, value)

    if lanip:
        if "192.168.2.1" not in dictionary:
            fail("default LAN address not found in dictionary")
        dictionary = dictionary.replace("192.168.2.1", lanip)

    defaults_path.write_text(defaults, encoding="utf-8")
    if lanip:
        dict_path.write_text(dictionary, encoding="utf-8")

    print(f"Applied {len(macros)} validated firmware default(s).")


if __name__ == "__main__":
    main()
