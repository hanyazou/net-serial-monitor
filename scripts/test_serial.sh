#!/bin/bash

# Simple probe: open the device and do a quick check (customize as needed)
DEV="${1:-/dev/ttyUSB0}"

# Example: just test we can open it for reading/writing
if [ -c "$DEV" ] && exec 3<>"$DEV"; then
  # Optional: send a probe and read a short reply here
  # printf 'PING\r\n' >&3; sleep 0.1; head -c 1 <&3 >/dev/null 2>&1
  exec 3>&-
  exit 0
fi

exit 1
