#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 Stephen Hemminger

# Run a memif server and client testpmd pair over a shared socket and
# check that traffic flows in both directions without descriptor errors.
# This is the black box connectivity and regression baseline for the
# memif peer request validation; adversarial (fuzz) peers can be added
# as a separate driver against the same two-instance setup.

build=${1:-build} # first argument can be the build directory
testpmd=$1 # or first argument can be the testpmd path
srvcores=${2:-0-1} # cores for the server instance
clicores=${3:-2-3} # cores for the client instance
eal_options=$4
testpmd_options=$5

[ -f "$testpmd" ] && build=$(dirname $(dirname $testpmd))
[ -f "$testpmd" ] || testpmd=$build/app/dpdk-testpmd
[ -f "$testpmd" ] || testpmd=$build/app/testpmd
if [ ! -f "$testpmd" ] ; then
	echo 'ERROR: testpmd cannot be found' >&2
	exit 1
fi

if ldd $testpmd | grep -q librte_ ; then
	export LD_LIBRARY_PATH=$build/lib:$LD_LIBRARY_PATH
	libs="-d $build/drivers"
else
	libs=
fi

# Skip (do not fail) where the driver was not built, for example a
# build restricted with -Denable_drivers.
config=$build/rte_build_config.h
if [ -f "$config" ] && ! grep -q '^#define RTE_NET_MEMIF ' $config ; then
	echo 'SKIP: memif driver is not built' >&2
	exit 77 # automake convention for a skipped test
fi

# The server and client run as two separate primary processes, needing
# two cores each. Skip (do not fail) where there are not enough cores.
# Use nproc rather than getconf, it respects the affinity mask.
ncpus=$(nproc 2>/dev/null || echo 1)
if [ "$ncpus" -lt 4 ] ; then
	echo "SKIP: memif test needs 4 cores, only $ncpus online" >&2
	exit 77
fi

# Per run temporary socket and logs, cleaned up on exit.
rundir=$(mktemp -d)
sock=$rundir/memif.sock
srvlog=$rundir/server.log
clilog=$rundir/client.log
srvpid=

cleanup()
{
	[ -n "$srvpid" ] && kill $srvpid 2>/dev/null || true
	rm -rf $rundir
}
trap cleanup EXIT

common="--no-huge -m 64 --in-memory --file-prefix"

# Use a pathname (non abstract) socket so its creation can be waited on
# and so it is removed with the run directory on exit.
vdev_srv="net_memif0,role=server,socket-abstract=no,socket=$sock"
vdev_cli="net_memif0,role=client,socket-abstract=no,socket=$sock"

# Both sides forward what they receive back to the peer, so that the
# client to server (C2S) and server to client (S2C) rings both carry
# traffic; descriptors supplied by the client are validated on both.
# The server starts first, it listens on the socket.
$testpmd $common memif_srv -l $srvcores $libs \
	--vdev=$vdev_srv $eal_options -- \
	--no-mlockall --total-num-mbufs=8192 \
	--forward-mode=macswap --auto-start --stats-period 1 \
	$testpmd_options > $srvlog 2>&1 &
srvpid=$!

# Wait for the server to create the listening socket (up to ~5s).
tries=0
while [ ! -S "$sock" ] ; do
	tries=$((tries + 1))
	if [ $tries -gt 50 ] ; then
		echo 'ERROR: server socket not created' >&2
		cat $srvlog >&2
		exit 1
	fi
	sleep 0.1 2>/dev/null || sleep 1
done

# The client seeds the first burst with "start tx_first" and then
# forwards what comes back, so the packets keep going round. It is
# interactive (-i, not -ia) so that the burst is sent by that command
# rather than by an auto-start with nothing to forward yet. Keep going
# on a non-zero exit so that the logs below are still reported.
clistatus=0
(echo 'start tx_first' && sleep 3 && echo stop) | \
$testpmd $common memif_cli -l $clicores $libs \
	--vdev=$vdev_cli $eal_options -- \
	--no-mlockall --total-num-mbufs=8192 \
	--forward-mode=io --stats-period 1 \
	$testpmd_options -i > $clilog 2>&1 || clistatus=$?

# Let the server drain and print a final stats block, then stop it.
sleep 1
kill $srvpid 2>/dev/null || true
wait $srvpid 2>/dev/null || true
srvpid=

fail=0

if [ $clistatus -ne 0 ] ; then
	echo "ERROR: client exited with status $clistatus" >&2
	fail=1
fi

# testpmd prints a periodic statistics block per port and a final forward
# statistics block. Match any line showing a non-zero count rather than
# the last one, so that the result does not depend on when each instance
# happened to be stopped.
nonzero()
{
	grep "$2" "$1" | grep -q "$2"'[[:space:]]*[^0[:space:]]'
}

# Both rings must carry traffic: the client drives the client to server
# ring and the server sends the same packets back over server to client.
check_nonzero() # log pattern description
{
	if ! nonzero "$1" "$2" ; then
		echo "ERROR: $3" >&2
		fail=1
	fi
}

check_nonzero $clilog 'TX-packets: ' 'client did not transmit any packet'
check_nonzero $srvlog 'RX-packets: ' 'server did not receive any packet'
check_nonzero $srvlog 'TX-packets: ' 'server did not transmit any packet'
check_nonzero $clilog 'RX-packets: ' 'client did not receive any packet'

# A conforming peer must not trip descriptor validation: no rx/tx errors
# and no "bad descriptor" log line on either side.
for log in $srvlog $clilog ; do
	if nonzero $log 'RX-errors: ' ; then
		echo "ERROR: RX-errors reported in $log" >&2
		fail=1
	fi
	if nonzero $log 'TX-errors: ' ; then
		echo "ERROR: TX-errors reported in $log" >&2
		fail=1
	fi
	if grep -q 'bad descriptor' $log ; then
		echo "ERROR: descriptor validation rejected a valid request in $log" >&2
		fail=1
	fi
done

if [ $fail -ne 0 ] ; then
	echo '--- server log ---' >&2
	cat $srvlog >&2
	echo '--- client log ---' >&2
	cat $clilog >&2
	exit 1
fi

echo 'memif server/client forwarding: OK'
