#!/bin/sh
echo 'GROUP (' $(echo $* | xargs -n1 basename | sort | xargs) ')'
