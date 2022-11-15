#!/bin/sh

set -eu

if [ "$#" -lt 1 ]; then
	echo "No branch selected, using current branch"
fi

for f in "$@"; do
	cd ../lfht
	git stash
	git checkout "$f"
	cd ../lfht-test

	make clean bench
	mv bench "$f"
done

