#!/bin/sh

make test >/dev/null || exit 1

export LD_PRELOAD=./libjemalloc.so
src="$(find . -name 'test.c' | head -n1)"
til="$(grep -E '\s*case [0-9]*:' "$src" | sed 's;.*case \([0-9]*\).*;\1;g' | sort -V | tail -n1)"

if [ -n "$1" ]; then
	./test "$1" "$(nproc)"
	exit
fi

i=0
while [ "$i" -lt "$til" ]; do
	i=$(( i + 1 ))
	./test "$i" "$(nproc)" 2>/dev/null
done

