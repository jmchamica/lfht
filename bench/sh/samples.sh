#!/bin/sh

set -eu

if [ "$#" -le 1 ]; then
	echo "$0 <number of samples> <command>"
	exit 1
fi

#export LD_PRELOAD=./libjemalloc.so
til="$1"
com="$2"
cores="$3"

tmp=.tmp

# $1 -> core count
getmask() {
	echo "0-$(( $1 - 1 ))"
}

# $1 -> pattern to find
avg() {
	values="$(grep "$1" "$tmp" | sed 's;.* ;;g' | xargs)"
	count="$(echo "$values" | wc -w)"
	sum="($(echo "$values" | sed 's; ;+;g'))"
	if echo $sum | grep "nan" >/dev/null 2>&1; then echo "$(echo "$1" | sed 's;\^;;g'): 0 ~0"; return ; fi
	mean="$(python -c "print(\"{:.7f}\".format($sum/$count))")"
	if echo $mean | grep "nan" >/dev/null 2>&1; then echo "$(echo "$1" | sed 's;\^;;g'): 0 ~0"; return ; fi
	var="($(for n in $values; do printf "((%s-%s)**2)" "$n" "$mean"; done | sed 's;)(;)+(;g'))"
	if echo $var | grep "nan" >/dev/null 2>&1; then echo "$(echo "$1" | sed 's;\^;;g'): $mean ~0"; return ; fi
	std="$(python -c "import math; print(\"{:.7f}\".format(math.sqrt($var/$count)))")"
	if echo $std | grep "nan" >/dev/null 2>&1; then echo "$(echo "$1" | sed 's;\^;;g'): $mean ~0"; return ; fi

	# average
	echo "$(echo "$1" | sed 's;\^;;g'): $mean ~$std"
}

trap "rm \"$tmp\"" 0
trap "rm \"$tmp\"" 1
trap "rm \"$tmp\"" 2

i=0
while [ "$i" -lt "$til" ]; do
	i=$(( i + 1 ))
	printf "(%d/%d)\n" "$i" "$til" >&2
	if [ "$cores" -le 16 ]; then
		cores=16
	else
		cores=32
	fi
	taskset -c "$(getmask "$cores")" $com >&2 2>>"$tmp"
done

avg '^Real Time (s)'
avg 'Process Time (s)'
grep 'Cores' "$tmp" | head -n1
grep 'Test size' "$tmp" | head -n1
if grep 'Frozen' "$tmp" >/dev/null 2>&1; then
	avg 'Operations'
	avg 'Throughput'
	avg 'Unfrozen'
	avg 'Frozen'
	avg 'Compressed'
	avg 'Compressions rolledback'
	avg 'Expanded'
	avg 'Failure ratio'
	avg 'Max depth'
	avg 'Paths Traversed'
	avg 'Average path length'
fi
