#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016 6WIND S.A.

print_usage ()
{
	echo "usage: $(basename $0) [-h] <git_range>"
}

print_help ()
{
	print_usage
	cat <<- END_OF_HELP

	Find fixes to backport on previous versions.
	It looks for a tag "Fixes" or for "Cc: stable@dpdk.org".
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
	# use current branch as history reference
	local refbranch=$(git rev-parse --abbrev-ref HEAD)
	local tag=$( (git tag -l --contains $1 --merged $refbranch 2>&- ||
		# tag --merged option has been introduced in git 2.7.0
		# below is a fallback in case of old git version
		for t in $(git tag -l --contains $1) ; do
			git branch $refbranch --contains $t |
			sed "s,.\+,$t,"
		done) |
		head -n1)
	if [ -z "$tag" ] ; then
		# before -rc1 tag of release in progress
		cat VERSION | cut -d'.' -f-2
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
		local roothashes="$(origin_filter $origin)"
		if [ -n "$roothashes" ] ; then
			# look chained fix of fix recursively
			local rootver="$(origin_version $roothashes)"
			[ -n "$rootver" ] || continue
			echo "$rootver (partially fixed in $origin @ $origver)"
		else
			echo "$origver"
		fi
	# filter the oldest origin
	done | sort -uV | head -n1
}

# print a marker for stable tag presence
stable_tag () # <hash>
{
	if git log --format='%b' -1 $1 | grep -qi '^Cc: *stable@dpdk.org' ; then
		echo 'S'
	else
		echo '-'
	fi
}

# print a marker for fixes tag presence
fixes_tag () # <hash>
{
        if git log --format='%b' -1 $1 | grep -qi '^Fixes: *' ; then
                echo 'F'
        else
                echo '-'
        fi
}

git log --oneline --reverse $range |
while read id headline ; do
	origins=$(origin_filter $id)
	stable=$(stable_tag $id)
	fixes=$(fixes_tag $id)
	[ "$stable" = "S" ] || [ "$fixes" = "F" ] || [ -n "$origins" ] || continue
	version=$(commit_version $id)
	if [ -n "$origins" ] ; then
		origver="$(origin_version $origins)"
		[ -n "$origver" ] || continue
		# ignore fix of bug introduced in the same release
		! echo "$origver" | grep -q "^$version" || continue
	else
		origver='N/A'
	fi
	printf '%s %7s %s %s %s (%s)\n' $version $id $stable $fixes "$headline" "$origver"
done
