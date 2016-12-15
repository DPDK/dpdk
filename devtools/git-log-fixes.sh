#! /bin/sh -e

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

print_usage ()
{
	echo "usage: $(basename $0) [-h] <git_range>"
}

print_help ()
{
	print_usage
	cat <<- END_OF_HELP

	Find fixes to backport on previous versions.
	It looks for the word "fix" in the headline or a tag "Fixes" or "Reverts".
	The oldest bug origin is printed as well as partially fixed versions.
	END_OF_HELP
}

usage_error () # <message>
{
	echo "$*" >&2
	print_usage >&2
	exit 1
}

while getopts h ARG ; do
	case $ARG in
		h ) print_help ; exit 0 ;;
		? ) print_usage >&2 ; exit 1 ;;
	esac
done
shift $(($OPTIND - 1))
[ $# -ge 1 ] || usage_error 'range argument required'
range="$*"

# get major release version of a commit
commit_version () # <hash>
{
	tag=$(git tag -l --contains $1 | head -n1)
	if [ -z "$tag" ] ; then
		# before -rc1 tag of release in progress
		make showversion | cut -d'.' -f-2
	else
		echo $tag | sed 's,^v,,' | sed 's,-rc.*,,'
	fi
}

# get bug origin hashes of a fix
origin_filter () # <hash>
{
	git log --format='%b' -1 $1 |
	sed -n 's,^ *\([Ff]ixes\|[Rr]everts\): *\([0-9a-f]*\).*,\2,p'
}

# get oldest major release version of bug origins
origin_version () # <origin_hash> ...
{
	for origin in $* ; do
		# check hash is valid
		git rev-parse -q --verify $1 >&- || continue
		# get version of this bug origin
		local origver=$(commit_version $origin)
		local roothashes=$(origin_filter $origin)
		if [ -n "$roothashes" ] ; then
			# look chained fix of fix recursively
			local rootver="$(origin_version $roothashes)"
			[ -n "$rootver" ] || continue
			echo "$rootver (partially fixed in $origver)"
		else
			echo "$origver"
		fi
	# filter the oldest origin
	done | sort -uV | head -n1
}

git log --oneline --reverse $range |
while read id headline ; do
	origins=$(origin_filter $id)
	[ -n "$origins" ] || echo "$headline" | grep -q fix || continue
	version=$(commit_version $id)
	if [ -n "$origins" ] ; then
		origver="$(origin_version $origins)"
		[ -n "$origver" ] || continue
		# ignore fix of bug introduced in the same release
		! echo "$origver" | grep -q "^$version" || continue
	else
		origver='N/A'
	fi
	printf '%s %7s %s (%s)\n' $version $id "$headline" "$origver"
done
