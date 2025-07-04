# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 Arnon Warshavsky <arnon@qwilt.com>

# This awk script receives a list of expressions to monitor
# and a list of folders to search these expressions in
# - No search is done inside comments
# - Both additions and removals of the expressions are checked
#   A positive balance of additions fails the check

BEGIN {
	split(FOLDERS,deny_folders," ");
	split(EXPRESSIONS,deny_expr," ");
	split(SKIP_FILES,skip_files," ");
	in_file=0;
	in_comment=0;
	count=0;
	warned=0;
	comment_start="/*"
	comment_end="*/"
}
# search for add/remove instances in current file
# state machine assumes the comments structure is enforced by
# checkpatches.pl
(in_file) {
	if ($0 ~ "^@@") {
		in_comment = 0
	}
	# comment start
	if (index($0,comment_start) > 0) {
		in_comment = 1
	}
	# non comment code
	if (in_comment == 0) {
		for (i in deny_expr) {
			forbidden_added = "^\\+.*" deny_expr[i];
			forbidden_removed="^-.*" deny_expr[i];
			if ($0 ~ forbidden_added) {
				count = count + 1
			}
			if ($0 ~ forbidden_removed) {
				count = count - 1
			}
		}
	}
	# comment end
	if (index($0,comment_end) > 0) {
		in_comment = 0
	}
}
# switch to next file , check if the balance of add/remove
# of previous file had new additions
($0 ~ "^\\+\\+\\+ b/") {
	in_file = 0;
	if (count > 0) {
		warned = warned + 1
		print "Warning in " substr(last_file,7) ":"
	}
	count = 0
	for (i in deny_folders) {
		if (!($0 ~ "^\\+\\+\\+ b/" deny_folders[i])) {
			continue
		}
		skip = 0
		for (j in skip_files) {
			if (!($0 ~ "^\\+\\+\\+ b/" skip_files[j])) {
				continue
			}
			skip = 1
			break
		}
		if (skip == 0) {
			in_file = 1
			last_file = $0
		}
		break
	}
}
END {
	if (count > 0) {
		warned = warned + 1
		print "Warning in " substr(last_file,7) ":"
	}
	if (warned > 0) {
		print MESSAGE
		exit RET_ON_FAIL
	}
}
