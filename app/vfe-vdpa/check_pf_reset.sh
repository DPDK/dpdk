#!/bin/bash

FILE="/tmp/pf_resetting"

echo "Start to wait PF reset completes by checking $FILE."

# Check if the file exists
while [ -e "$FILE" ]; do
    echo "Waitting for PF reset to complete"
done

echo "PF does not start yet or PF reset completes ($FILE does not exist)."