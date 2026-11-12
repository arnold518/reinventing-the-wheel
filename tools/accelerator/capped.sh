#!/bin/bash
# capped.sh MEMORY_MB command...: run a command in a memory-capped user scope
# without swap, so an overrun stops only that command (this host reboots on OOM).
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}
export DBUS_SESSION_BUS_ADDRESS=${DBUS_SESSION_BUS_ADDRESS:-unix:path=$XDG_RUNTIME_DIR/bus}
mem=$1; shift
exec systemd-run --user --scope --quiet -p MemoryMax=${mem}M -p MemorySwapMax=0 "$@"
