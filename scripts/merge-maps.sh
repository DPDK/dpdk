#!/bin/sh

FILES=$(find "$RTE_SDK"/lib "$RTE_SDK"/drivers -name "*_version.map")
SYMBOLS=$(grep -h "{" $FILES | sort -u | sed 's/{//')

first=0
prev_sym="none"

for s in $SYMBOLS; do
	echo "$s {"
	echo "    global:"
	echo ""
	for f in $FILES; do
		sed -n "/$s {/,/}/p" "$f" | sed '/^$/d' | grep -v global | grep -v local | sed -e '1d' -e '$d'
	done | sort -u
	echo ""
	if [ $first -eq 0 ]; then
		first=1;
		echo "    local: *;";
	fi
	if [ "$prev_sym" = "none" ]; then
		echo "};";
		prev_sym=$s;
	else
		echo "} $prev_sym;";
		prev_sym=$s;
	fi
	echo ""
done
