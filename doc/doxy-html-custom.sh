#! /bin/sh -e

CSS=$1

# space between item and its comment
echo 'dd td:first-child {padding-right: 2em;}' >> $CSS
