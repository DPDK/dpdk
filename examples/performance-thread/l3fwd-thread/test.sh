#!/bin/bash

case "$1" in

	######################
	# 1 L-core per pcore #
	######################

	"1.1")
		echo "1.1 1 L-core per pcore (N=2)"

		./build/l3fwd-thread -c ff -n 2 -- -P -p 3 \
				--enable-jumbo --max-pkt-len 1500  \
				--rx="(0,0,0,0)(1,0,0,0)"          \
				--tx="(1,0)"                       \
				--stat-lcore 2                     \
				--no-lthread

		;;

	"1.2")
		echo "1.2 1 L-core per pcore (N=4)"

		./build/l3fwd-thread -c ff -n 2 -- -P -p 3 \
				--enable-jumbo --max-pkt-len 1500  \
				--rx="(0,0,0,0)(1,0,1,1)"          \
				--tx="(2,0)(3,1)"                  \
				--stat-lcore 4                     \
				--no-lthread
		;;

	"1.3")
		echo "1.3 1 L-core per pcore (N=8)"

		./build/l3fwd-thread -c 1ff -n 2 -- -P -p 3                          \
				--enable-jumbo --max-pkt-len 1500                            \
				--rx="(0,0,0,0)(0,1,1,1)(1,0,2,2)(1,1,3,3)"                  \
				--tx="(4,0)(5,1)(6,2)(7,3)"                                  \
				--stat-lcore 8                                               \
				--no-lthread
		;;

	"1.4")
		echo "1.3 1 L-core per pcore (N=16)"

		./build/l3fwd-thread -c 3ffff -n 2 -- -P -p 3                          \
				--enable-jumbo --max-pkt-len 1500                              \
				--rx="(0,0,0,0)(0,1,1,1)(0,2,2,2)(0,3,3,3)(1,0,4,4)(1,1,5,5)(1,2,6,6)(1,3,7,7)" \
				--tx="(8,0)(9,1)(10,2)(11,3)(12,4)(13,5)(14,6)(15,7)"          \
				--stat-lcore 16                                                \
				--no-lthread
		;;


	######################
	# N L-core per pcore #
	######################

	"2.1")
		echo "2.1 N L-core per pcore (N=2)"

		./build/l3fwd-thread -c ff -n 2 --lcores="2,(0-1)@0" -- -P -p 3 \
				--enable-jumbo --max-pkt-len 1500                       \
				--rx="(0,0,0,0)(1,0,0,0)"                               \
				--tx="(1,0)"                                            \
				--stat-lcore 2                                          \
				--no-lthread

		;;

	"2.2")
		echo "2.2 N L-core per pcore (N=4)"

		./build/l3fwd-thread -c ff -n 2 --lcores="(0-3)@0,4" -- -P -p 3 \
				--enable-jumbo --max-pkt-len 1500  \
				--rx="(0,0,0,0)(1,0,1,1)"          \
				--tx="(2,0)(3,1)"                  \
				--stat-lcore 4                     \
				--no-lthread
		;;

	"2.3")
		echo "2.3 N L-core per pcore (N=8)"

		./build/l3fwd-thread -c 3ffff -n 2 --lcores="(0-7)@0,8" -- -P -p 3     \
				--enable-jumbo --max-pkt-len 1500                              \
				--rx="(0,0,0,0)(0,1,1,1)(1,0,2,2)(1,1,3,3)"                    \
				--tx="(4,0)(5,1)(6,2)(7,3)"                                    \
				--stat-lcore 8                                                 \
				--no-lthread
		;;

	"2.4")
		echo "2.3 N L-core per pcore (N=16)"

		./build/l3fwd-thread -c 3ffff -n 2 --lcores="(0-15)@0,16" -- -P -p 3   \
				--enable-jumbo --max-pkt-len 1500                              \
				--rx="(0,0,0,0)(0,1,1,1)(0,2,2,2)(0,3,3,3)(1,0,4,4)(1,1,5,5)(1,2,6,6)(1,3,7,7)" \
				--tx="(8,0)(9,1)(10,2)(11,3)(12,4)(13,5)(14,6)(15,7)"          \
				--stat-lcore 16                                                \
				--no-lthread
		;;


	#########################
	# N L-threads per pcore #
	#########################

	"3.1")
		echo "3.1 N L-threads per pcore (N=2)"

		./build/l3fwd-thread -c ff -n 2 -- -P -p 3  \
				--enable-jumbo --max-pkt-len 1500   \
				--rx="(0,0,0,0)(1,0,0,0)"           \
				--tx="(0,0)"                        \
				--stat-lcore 1
		;;

	"3.2")
		echo "3.2 N L-threads per pcore (N=4)"

		./build/l3fwd-thread -c ff -n 2 -- -P -p 3  \
				--enable-jumbo --max-pkt-len 1500   \
				--rx="(0,0,0,0)(1,0,0,1)"           \
				--tx="(0,0)(0,1)"                   \
				--stat-lcore 1
		;;

	"3.3")
		echo "3.2 N L-threads per pcore (N=8)"

		./build/l3fwd-thread -c ff -n 2 -- -P -p 3                             \
				--enable-jumbo --max-pkt-len 1500                              \
				--rx="(0,0,0,0)(0,1,0,1)(1,0,0,2)(1,1,0,3)"                    \
				--tx="(0,0)(0,1)(0,2)(0,3)"                                    \
				--stat-lcore 1
		;;

	"3.4")
		echo "3.2 N L-threads per pcore (N=16)"

		./build/l3fwd-thread -c ff -n 2 -- -P -p 3                             \
				--enable-jumbo --max-pkt-len 1500                              \
				--rx="(0,0,0,0)(0,1,0,1)(0,2,0,2)(0,0,0,3)(1,0,0,4)(1,1,0,5)(1,2,0,6)(1,3,0,7)" \
				--tx="(0,0)(0,1)(0,2)(0,3)(0,4)(0,5)(0,6)(0,7)"                \
				--stat-lcore 1
		;;

esac
