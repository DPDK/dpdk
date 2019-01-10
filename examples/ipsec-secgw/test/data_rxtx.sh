#! /bin/bash

TCP_PORT=22222

ping_test1()
{
	dst=$1

	i=0
	st=0
	while [[ $i -ne 1200 && $st -eq 0 ]];
	do
		let i++
		ping -c 1 -s ${i} ${dst}
		st=$?
	done

	if [[ $st -ne 0 ]]; then
		echo "ERROR: $0 failed for dst=${dst}, sz=${i}"
	fi
	return $st;
}

ping6_test1()
{
	dst=$1

	i=0
	st=0
	while [[ $i -ne 1200 && $st -eq 0 ]];
	do
		let i++
		ping6 -c 1 -s ${i} ${dst}
		st=$?
	done

	if [[ $st -ne 0 ]]; then
		echo "ERROR: $0 failed for dst=${dst}, sz=${i}"
	fi
	return $st;
}

scp_test1()
{
	dst=$1

	for sz in 1234 23456 345678 4567890 56789102 ; do
		x=`basename $0`.${sz}
		dd if=/dev/urandom of=${x} bs=${sz} count=1
		scp ${x} [${dst}]:${x}
		scp [${dst}]:${x} ${x}.copy1
		diff -u ${x} ${x}.copy1
		st=$?
		rm -f ${x} ${x}.copy1
		ssh ${REMOTE_HOST} rm -f ${x}
		if [[ $st -ne 0 ]]; then
			return $st
		fi
	done

	return 0;
}
