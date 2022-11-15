#!/bin/sh

set -eu

NPROC="$(nproc)"

def_chain_size=2
def_hash_size=3
dir="./data"

# $1 -> name of output file
#nodes="100000000"
nodes="100"
samples=10

mkdir -p "$dir"
rm "$dir"/* >/dev/null 2>&1 | true

sample() {
	prog="$1"
	test_size="$2"
	chain_size="$3"
	hash_size="$4"
	ratios="$5"
	target="$6"
	cores="$7"
	com="$prog $test_size $cores $chain_size $hash_size $ratios"

	echo "./samples.sh $samples '$com' $cores"
	sh -c "./samples.sh $samples '$com' $cores" >tmp
	time="$(cat tmp | grep 'Real Time (s)' | sed 's;Real Time (s): ;;g' | sed 's; .*;;g')"
	etime="$(cat tmp | grep 'Real Time (s)' | sed 's;Real Time (s): ;;g' | sed 's;.*~;;g')"
	echo "Time ($cores cores): $time"
	echo "$cores $time $etime" >>"$dir/$target"

	# the realease version will not contain these stats
	if cat tmp | grep 'Compressed' >/dev/null 2>&1; then
		put="$(cat tmp | grep 'Throughput' | sed 's;.*: ;;g' | sed 's; .*;;g')"
		eput="$(cat tmp | grep 'Throughput' | sed 's;.*: ;;g' | sed 's;.*~;;g')"
		echo "Throughput ($cores cores): $put"
		echo "$cores $put $eput" >>"$dir/$target-put"

		stat="$(cat tmp | grep 'Compressed' | sed 's;Compressed: ;;g' | sed 's; .*;;g')"
		echo "$cores $stat" >>"$dir/compressed-$target"

		stat="$(cat tmp | grep 'Expanded' | sed 's;Expanded: ;;g' | sed 's; .*;;g')"
		echo "$cores $stat" >>"$dir/expanded-$target"

		stat="$(cat tmp | grep 'Failure ratio' | sed 's;Failure ratio: ;;g' | sed 's; .*;;g')"
		echo "$cores $stat" >>"$dir/fail-$target"

		stat="$(cat tmp | grep 'Paths Traversed' | sed 's;Paths Traversed: ;;g' | sed 's; .*;;g')"
		echo "$cores $stat" >>"$dir/paths-$target"

		stat="$(cat tmp | grep 'Average path length' | sed 's;Average path length: ;;g' | sed 's; .*;;g')"
		echo "$cores $stat" >>"$dir/apl-$target"
	fi
}

unit() {
	prog="$1"
	test_size="$2"
	chain_size="$3"
	hash_size="$4"
	ratios="$5"
	target="$6"

	c=1
	for c in 1 4 8 16 24 32; do
		if [ "$c" -gt "$NPROC" ]; then
			return
		fi
		values=""
		i=0

		sample "$prog" "$test_size" "$chain_size" "$hash_size" "$ratios" "$target" "$c"

		rm tmp
		c=$(( c + 1 ))
	done
}

# $1 -> ratios
# $2 -> test name
dobench() {
	ratios="$1"
	test_name="$2"

	for f in nocompress freeze counter role; do
		if echo "$f" | grep ".sh" >/dev/null 2>&1; then
			# exclude script
			continue
		fi

		echo "$f file"
		file="$test_name-$f"
		sfile="$dir/speedup-$test_name-$f"
		efile="$dir/efficiency-$test_name-$f"
		unit "./$f" "$nodes" "$def_chain_size" "$def_hash_size" "$ratios" "$file"
		file="$dir/$file"

		seq="$(cat "$dir/$test_name-nocompress" | head -n1 | awk '{print $2}')"

		echo "$f file (speedup)"
		cat "$file" | while read l; do
			# calc speedup and efficiency
			c="$(echo "$l" | awk '{print $1}')"
			r="$(echo "$l" | awk '{print $2}')"
			echo "$c $(python -c "print(\"{:.7f}\".format($seq/$r))")"
		done | tee "$sfile"
		printf "\n"
	done
}

echo
echo "Half Searches"
dobench "0.25 0.25 0.25 0.25" "half-searches"

echo
echo "Insert All"
dobench "1 0 0 0" "insert-all"

echo
echo "Remove All"
dobench "0 1 0 0" "remove-all-preinserted"

echo
echo "Search All"
dobench "0 0 1 0" "search-all-preinserted"

echo
echo "Search All Empty"
dobench "0 0 0 1" "search-all-empty"
