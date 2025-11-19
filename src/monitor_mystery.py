#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import time
import sys
from datetime import datetime

import requests

URL = "https://oj.cs.tsinghua.edu.cn/thuscc/api/ranklist"
INTERVAL = 60


def now_str() -> str:
  """Return current time string."""
  return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def fetch_ranklist():
  """Fetch ranklist JSON from server."""
  try:
    resp = requests.get(URL, timeout = 5)
    resp.raise_for_status()
    return resp.json()
  except Exception as e:
    print(f"[{now_str()}] Error fetching ranklist: {e}", file=sys.stderr)
    return None


def check_mystery(data: dict, seen: set):
  """Check all teams' MYSTERY perfValues and print alerts if < 1."""
  teams = data.get("data", [])
  for team in teams:
    team_name = team.get("teamName", "")
    rank = team.get("rank", "?")

    perf_values = team.get("perfValues") or {}
    if "MYSTERY" not in perf_values:
      continue

    value = perf_values["MYSTERY"]

    try:
      v = float(value)
    except (TypeError, ValueError):
      continue

    if v < 1.0:
      key = (team_name, v)
      if key in seen:
        continue
      seen.add(key)

      print(f"[{now_str()}] MYSTERY perfValue < 1 detected:")
      print(f"  rank: {rank}")
      print(f"  teamName: {team_name}")
      print(f"  MYSTERY perfValue: {v}")
      print("-" * 40)


def main():
  """Main monitoring loop."""
  seen = set()
  print(f"[{now_str()}] Start monitoring {URL}")
  print(f"Check interval: {INTERVAL} seconds. Press Ctrl+C to stop.")

  while True:
    data = fetch_ranklist()
    if data is not None:
      check_mystery(data, seen)
    time.sleep(INTERVAL)


if __name__ == "__main__":
  main()
