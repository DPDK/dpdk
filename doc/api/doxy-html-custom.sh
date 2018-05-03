#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2013 6WIND S.A.

CSS=$1

# space between item and its comment
echo 'dd td:first-child {padding-right: 2em;}' >> $CSS
