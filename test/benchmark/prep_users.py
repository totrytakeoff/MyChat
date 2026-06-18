#!/usr/bin/env python3
"""Pre-register benchmark users and dump tokens to JSON."""

import argparse
import json
import sys
import time
import urllib.request
import urllib.error
import urllib.parse

API_BASE = "http://{host}:{port}/api/v1"


def req(method, url, data=None):
    headers = {"Content-Type": "application/json"}
    body = json.dumps(data).encode() if data else None
    r = urllib.request.Request(url, data=body, headers=headers, method=method)
    try:
        with urllib.request.urlopen(r, timeout=10) as resp:
            return json.loads(resp.read())
    except urllib.error.HTTPError as e:
        return {"error": f"HTTP {e.code}: {e.read().decode()}"}
    except Exception as e:
        return {"error": str(e)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=10002)
    ap.add_argument("-n", "--count", type=int, default=10, help="Number of users to create")
    ap.add_argument("-o", "--output", default="users.json", help="Output token file")
    ap.add_argument("--prefix", default="bench", help="Account prefix")
    ap.add_argument("--password", default="pass1234")
    ap.add_argument("--device-id", default="bench-device")
    ap.add_argument("--platform", default="bench")
    args = ap.parse_args()

    base = API_BASE.format(host=args.host, port=args.port)
    tokens = []

    for i in range(1, args.count + 1):
        account = f"{args.prefix}_{i:04d}"
        register_data = {
            "account": account,
            "password": args.password,
            "nickname": f"BenchUser{i}",
        }

        r = req("POST", f"{base}/auth/register", register_data)
        if "error" in r and "already exists" not in str(r.get("error", "")):
            print(f"[{i}/{args.count}] register {account}: {r.get('error', '')}", file=sys.stderr)
        else:
            print(f"[{i}/{args.count}] register {account}: OK", file=sys.stderr)

        login_data = {
            "account": account,
            "password": args.password,
            "device_id": args.device_id,
            "platform": args.platform,
        }
        r = req("POST", f"{base}/auth/login", login_data)
        if "access_token" in r:
            tokens.append({
                "uid": r.get("profile", {}).get("uid", account),
                "account": account,
                "access_token": r["access_token"],
                "refresh_token": r.get("refresh_token", ""),
                "device_id": args.device_id,
                "platform": args.platform,
            })
            print(f"[{i}/{args.count}] login {account}: OK", file=sys.stderr)
        else:
            print(f"[{i}/{args.count}] login {account}: {r.get('error', '')}", file=sys.stderr)

        time.sleep(0.05)

    with open(args.output, "w") as f:
        json.dump(tokens, f, indent=2)
    print(f"\nWrote {len(tokens)} tokens to {args.output}")


if __name__ == "__main__":
    main()
