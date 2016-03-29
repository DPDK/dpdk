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

headlines=$(git log --format='%s' $range)
bodylines=$(git log --format='%b' $range)
tags=$(git log --format='%b' $range | grep -i -e 'by *:' -e 'fix.*:')
fixes=$(git log --format='%h %s' $range | grep -i ': *fix' | cut -d' ' -f1)

# check headline format (spacing, no punctuation, no code)
bad=$(echo "$headlines" | grep \
	-e '	' \
	-e '^ ' \
	-e ' $' \
	-e '\.$' \
	-e '[,;!?&|]' \
	-e ':.*_' \
	-e '^[^:]*$' \
	-e ':[^ ]' \
	-e ' :' \
	| sed 's,^,\t,')
[ -z "$bad" ] || printf "Wrong headline format:\n$bad\n"

# check headline label for common typos
bad=$(echo "$headlines" | grep \
	-e '^example[:/]' \
	-e '^apps/' \
	-e '^testpmd' \
	-e 'test-pmd' \
	-e '^bond:' \
	| sed 's,^,\t,')
[ -z "$bad" ] || printf "Wrong headline label:\n$bad\n"

# check headline lowercase for first words
bad=$(echo "$headlines" | grep \
	-e '^.*[A-Z].*:' \
	-e ': *[A-Z]' \
	| sed 's,^,\t,')
[ -z "$bad" ] || printf "Wrong headline uppercase:\n$bad\n"

# check headline uppercase (Rx/Tx, VF, L2, MAC, Linux, ARM...)
bad=$(echo "$headlines" | grep \
	-e 'rx\|tx\|RX\|TX' \
	-e '\<[pv]f\>' \
	-e '\<l[234]\>' \
	-e ':.*\<dma\>' \
	-e ':.*\<pci\>' \
	-e ':.*\<mtu\>' \
	-e ':.*\<mac\>' \
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

# check headline length (60 max)
bad=$(echo "$headlines" | awk 'length>60 {print}' | sed 's,^,\t,')
[ -z "$bad" ] || printf "Headline too long:\n$bad\n"

# check body lines length (75 max)
bad=$(echo "$bodylines" | awk 'length>75 {print}' | sed 's,^,\t,')
[ -z "$bad" ] || printf "Line too long:\n$bad\n"

# check tags spelling
bad=$(echo "$tags" |
	grep -v '^\(Reported\|Suggested\|Signed-off\|Acked\|Reviewed\|Tested\)-by: [^,]* <.*@.*>$' |
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
	good="Fixes: $hash "$(git log --format='("%s")' -1 $hash 2>&-)
	printf "$fixtag" | grep -v "^$good$"
done | sed 's,^,\t,')
[ -z "$bad" ] || printf "Wrong 'Fixes' reference:\n$bad\n"
