#! /bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016 6WIND S.A.

# Check commit logs (headlines and references)
#
# If any doubt about the formatting, please check in the most recent history:
#	git log --format='%>|(15)%cr   %s' --reverse | grep -i <pattern>

if [ "$1" = '-h' -o "$1" = '--help' ] ; then
	cat <<- END_OF_HELP
	usage: $(basename $0) [-h] [range]

	Check commit log formatting.
	The git range can be specified as a "git log" option,
	e.g. -1 to check only the latest commit.
	The default range starts from origin/master to HEAD.
	END_OF_HELP
	exit
fi

selfdir=$(dirname $(readlink -f $0))
range=${1:-origin/master..}
# convert -N to HEAD~N.. in order to comply with git-log-fixes.sh getopts
if printf -- $range | grep -q '^-[0-9]\+' ; then
	range="HEAD$(printf -- $range | sed 's,^-,~,').."
fi

commits=$(git log --format='%h' --reverse $range)
headlines=$(git log --format='%s' --reverse $range)
bodylines=$(git log --format='%b' --reverse $range)
fixes=$(git log --format='%h %s' --reverse $range | grep -i ': *fix' | cut -d' ' -f1)
stablefixes=$($selfdir/git-log-fixes.sh $range | sed '/(N\/A)$/d'  | cut -d' ' -f2)
tags=$(git log --format='%b' --reverse $range | grep -i -e 'by *:' -e 'fix.*:')
bytag='\(Reported\|Suggested\|Signed-off\|Acked\|Reviewed\|Tested\)-by:'

# check headline format (spacing, no punctuation, no code)
bad=$(echo "$headlines" | grep --color=always \
	-e '	' \
	-e '^ ' \
	-e ' $' \
	-e '\.$' \
	-e '[,;!?&|]' \
	-e ':.*_' \
	-e '^[^:]\+$' \
	-e ':[^ ]' \
	-e ' :' \
	| sed 's,^,\t,')
[ -z "$bad" ] || printf "Wrong headline format:\n$bad\n"

# check headline prefix when touching only drivers, e.g. net/<driver name>
bad=$(for commit in $commits ; do
	headline=$(git log --format='%s' -1 $commit)
	files=$(git diff-tree --no-commit-id --name-only -r $commit)
	[ -z "$(echo "$files" | grep -v '^\(drivers\|doc\|config\)/')" ] ||
		continue
	drv=$(echo "$files" | grep '^drivers/' | cut -d "/" -f 2,3 | sort -u)
	drvgrp=$(echo "$drv" | cut -d "/" -f 1 | uniq)
	if [ $(echo "$drvgrp" | wc -l) -gt 1 ] ; then
		echo "$headline" | grep -v '^drivers:'
	elif [ $(echo "$drv" | wc -l) -gt 1 ] ; then
		echo "$headline" | grep -v "^drivers/$drvgrp"
	else
		echo "$headline" | grep -v "^$drv"
	fi
done | sed 's,^,\t,')
[ -z "$bad" ] || printf "Wrong headline prefix:\n$bad\n"

# check headline label for common typos
bad=$(echo "$headlines" | grep --color=always \
	-e '^example[:/]' \
	-e '^apps/' \
	-e '^testpmd' \
	-e 'test-pmd' \
	-e '^bond:' \
	| sed 's,^,\t,')
[ -z "$bad" ] || printf "Wrong headline label:\n$bad\n"

# check headline lowercase for first words
bad=$(echo "$headlines" | grep --color=always \
	-e '^.*[[:upper:]].*:' \
	-e ': *[[:upper:]]' \
	| sed 's,^,\t,')
[ -z "$bad" ] || printf "Wrong headline uppercase:\n$bad\n"

# check headline uppercase (Rx/Tx, VF, L2, MAC, Linux, ARM...)
bad=$(echo "$headlines" | grep -E --color=always \
	-e ':.*\<(rx|tx|RX|TX)\>' \
	-e ':.*\<[pv]f\>' \
	-e ':.*\<[hsf]w\>' \
	-e ':.*\<l[234]\>' \
	-e ':.*\<api\>' \
	-e ':.*\<ARM\>' \
	-e ':.*\<(Aarch64|AArch64|AARCH64|Aarch32|AArch32|AARCH32)\>' \
	-e ':.*\<(Armv7|ARMv7|ArmV7|armV7|ARMV7)\>' \
	-e ':.*\<(Armv8|ARMv8|ArmV8|armV8|ARMV8)\>' \
	-e ':.*\<crc\>' \
	-e ':.*\<dma\>' \
	-e ':.*\<eeprom\>' \
	-e ':.*\<freebsd\>' \
	-e ':.*\<iova\>' \
	-e ':.*\<linux\>' \
	-e ':.*\<lro\>' \
	-e ':.*\<lsc\>' \
	-e ':.*\<mac\>' \
	-e ':.*\<mss\>' \
	-e ':.*\<mtu\>' \
	-e ':.*\<nic\>' \
	-e ':.*\<nvm\>' \
	-e ':.*\<numa\>' \
	-e ':.*\<pci\>' \
	-e ':.*\<phy\>' \
	-e ':.*\<pmd\>' \
	-e ':.*\<rss\>' \
	-e ':.*\<sctp\>' \
	-e ':.*\<tso\>' \
	-e ':.*\<udp\>' \
	-e ':.*\<[Vv]lan\>' \
	-e ':.*\<vdpa\>' \
	-e ':.*\<vsi\>' \
	| grep \
	-v ':.*\<OCTEON\ TX\>' \
	| sed 's,^,\t,')
[ -z "$bad" ] || printf "Wrong headline lowercase:\n$bad\n"

# special case check for VMDq to give good error message
bad=$(echo "$headlines" | grep -E --color=always \
	-e '\<(vmdq|VMDQ)\>' \
	| sed 's,^,\t,')
[ -z "$bad" ] || printf "Wrong headline capitalization, use 'VMDq':\n$bad\n"

# check headline length (60 max)
bad=$(echo "$headlines" |
	awk 'length>60 {print}' |
	sed 's,^,\t,')
[ -z "$bad" ] || printf "Headline too long:\n$bad\n"

# check body lines length (75 max)
bad=$(echo "$bodylines" | grep -v '^Fixes:' |
	awk 'length>75 {print}' |
	sed 's,^,\t,')
[ -z "$bad" ] || printf "Line too long:\n$bad\n"

# check starting commit message with "It"
bad=$(for commit in $commits ; do
	firstbodyline=$(git log --format='%b' -1 $commit | head -n1)
	echo "$firstbodyline" | grep --color=always -ie '^It '
done | sed 's,^,\t,')
[ -z "$bad" ] || printf "Wrong beginning of commit message:\n$bad\n"

# check tags spelling
bad=$(echo "$tags" |
	grep -v "^$bytag [^,]* <.*@.*>$" |
	grep -v '^Fixes: [0-9a-f]\{7\}[0-9a-f]* (".*")$' |
	sed 's,^.,\t&,')
[ -z "$bad" ] || printf "Wrong tag:\n$bad\n"

# check missing Fixes: tag
bad=$(for fix in $fixes ; do
	git log --format='%b' -1 $fix | grep -q '^Fixes: ' ||
		git log --format='\t%s' -1 $fix
done)
[ -z "$bad" ] || printf "Missing 'Fixes' tag:\n$bad\n"

# check Fixes: reference
IFS='
'
fixtags=$(echo "$tags" | grep '^Fixes: ')
bad=$(for fixtag in $fixtags ; do
	hash=$(echo "$fixtag" | sed 's,^Fixes: \([0-9a-f]*\).*,\1,')
	if git branch --contains $hash 2>&- | grep -q '^\*' ; then
		good="Fixes: $hash "$(git log --format='("%s")' -1 $hash 2>&-)
	else
		good="reference not in current branch"
	fi
	printf "$fixtag" | grep -v "^$good$"
done | sed 's,^,\t,')
[ -z "$bad" ] || printf "Wrong 'Fixes' reference:\n$bad\n"

# check Cc: stable@dpdk.org for fixes
bad=$(for fix in $stablefixes ; do
	git log --format='%b' -1 $fix | grep -qi '^Cc: *stable@dpdk.org' ||
		git log --format='\t%s' -1 $fix
done)
[ -z "$bad" ] || printf "Is it candidate for Cc: stable@dpdk.org backport?\n$bad\n"
