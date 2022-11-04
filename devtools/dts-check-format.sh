#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 University of New Hampshire

usage() {
	echo "Run formatting and linting programs for DTS. Usage:"

	# Get source code comments after getopts arguments and print them both
	grep -E '[a-zA-Z]+\) +#' "$0" | tr -d '#'
	exit 0
}

format=true
lint=true

# Comments after args serve as documentation; must be present
while getopts "hfl" arg; do
	case $arg in
	h) # Display this message
		usage
		;;
	f) # Don't run formatters
		format=false
		;;
	l) # Don't run linter
		lint=false
		;;
	*)
	esac
done


errors=0

if $format; then
	if command -v git > /dev/null; then
		if git rev-parse --is-inside-work-tree >&-; then
			echo "Formatting:"
			if command -v black > /dev/null; then
				echo "Formatting code with black:"
				black .
			else
				echo "black is not installed, not formatting"
				errors=$((errors + 1))
			fi
			if command -v isort > /dev/null; then
				echo "Sorting imports with isort:"
				isort .
			else
				echo "isort is not installed, not sorting imports"
				errors=$((errors + 1))
			fi

			git update-index --refresh
			retval=$?
			if [ $retval -ne 0 ]; then
				echo 'The "needs update" files have been reformatted.'
				echo 'Please update your commit.'
			fi
			errors=$((errors + retval))
		else
			echo ".git directory not found, not formatting code"
			errors=$((errors + 1))
		fi
	else
		echo "git command not found, not formatting code"
		errors=$((errors + 1))
	fi
fi

if $lint; then
	if $format; then
		echo
	fi
	echo "Linting:"
	if command -v pylama > /dev/null; then
		pylama .
		errors=$((errors + $?))
	else
		echo "pylama not found, unable to run linter"
		errors=$((errors + 1))
	fi
fi

echo
echo "Found $errors errors"
exit $errors
