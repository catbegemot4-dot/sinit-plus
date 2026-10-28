#!/bin/sh
# based on sinit
# Safe test: builds sinit+ with test/config.h and runs it as PID 1 inside
# its own PID namespace. Nothing on the real system is touched: reboot(2)
# in a PID namespace only terminates that namespace's init.
# Needs: a C compiler and util-linux unshare. Works as root or, if
# unprivileged user namespaces are enabled, as a normal user.
set -u
DIR=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d) || exit 1
trap 'rm -rf "$TMP"' EXIT

chmod +x "$DIR/test/rc.init" "$DIR/test/rc.shutdown" 2>/dev/null
cp "$DIR/sinit.c" "$DIR/test/config.h" "$TMP/" || exit 1
${CC:-cc} -D_DEFAULT_SOURCE -DTESTDIR="\"$DIR/test\"" -std=c99 \
	-Wall -Wextra -pedantic -Os -o "$TMP/sinit" "$TMP/sinit.c" || exit 1
echo "[test] built sinit, starting it as PID 1 in a new PID namespace"

rc=0
if [ "$(id -u)" -eq 0 ]; then
	unshare --pid --fork --mount-proc "$TMP/sinit" || rc=$?
else
	unshare --user --map-root-user --pid --fork --mount-proc "$TMP/sinit" || rc=$?
fi
echo "[test] sinit finished, exit status $rc (129 = namespace 'rebooted', 130 = 'powered off')"
