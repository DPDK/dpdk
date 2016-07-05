#! /bin/sh

# BSD LICENSE
#
# Copyright 2016 6WIND S.A.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of 6WIND S.A. nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

range=${1:-origin/master..}

commits=$(git log --format='%h' $range)
headlines=$(git log --format='%s' $range)
bodylines=$(git log --format='%b' $range)
fixes=$(git log --format='%h %s' $range | grep -i ': *fix' | cut -d' ' -f1)
tags=$(git log --format='%b' $range | grep -i -e 'by *:' -e 'fix.*:')
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
		echo "$headline" | grep -v "^$drvgrp"
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
	-e '^.*[A-Z].*:' \
	-e ': *[A-Z]' \
	| sed 's,^,\t,')
[ -z "$bad" ] || printf "Wrong headline uppercase:\n$bad\n"

# check headline uppercase (Rx/Tx, VF, L2, MAC, Linux, ARM...)
bad=$(echo "$headlines" | grep -E --color=always \
	-e '\<(rx|tx|RX|TX)\>' \
	-e '\<[pv]f\>' \
	-e '\<[hsf]w\>' \
	-e '\<l[234]\>' \
	-e ':.*\<api\>' \
	-e ':.*\<dma\>' \
	-e ':.*\<pci\>' \
	-e ':.*\<mtu\>' \
	-e ':.*\<mac\>' \
	-e ':.*\<numa\>' \
	-e ':.*\<vlan\>' \
	-e ':.*\<rss\>' \
	-e ':.*\<freebsd\>' \
	-e ':.*\<linux\>' \
	-e ':.*\<tilegx\>' \
	-e ':.*\<tile-gx\>' \
	-e ':.*\<arm\>' \
	-e ':.*\<armv7\>' \
	-e ':.*\<armv8\>' \
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

# check blank line after last Fixes: tag
bad=$(echo "$bodylines" |
	sed -n 'N;/\nFixes:/D;/\n$/D;/^Fixes:/P' |
	sed 's,^.,\t&,')
[ -z "$bad" ] || printf "Missing blank line after 'Fixes' tag:\n$bad\n"

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
	if git branch --contains $hash | grep -q '^\*' ; then
		good="Fixes: $hash "$(git log --format='("%s")' -1 $hash 2>&-)
	else
		good="reference not in current branch"
	fi
	printf "$fixtag" | grep -v "^$good$"
done | sed 's,^,\t,')
[ -z "$bad" ] || printf "Wrong 'Fixes' reference:\n$bad\n"
