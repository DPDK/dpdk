#
# run ./config/action.sh
#

p 1 action flow 0 meter 0 trtcm 1250000000 1250000000 1000000 1000000
p 1 action flow 0 policer 0 g G y Y r R
p 1 action flow 0 meter 1 trtcm 1250000000 1250000000 1000000 1000000
p 1 action flow 0 policer 1 g G y Y r R
p 1 action flow 0 meter 2 trtcm 1250000000 1250000000 1000000 1000000
p 1 action flow 0 policer 2 g G y Y r R
p 1 action flow 0 meter 3 trtcm 1250000000 1250000000 1000000 1000000
p 1 action flow 0 policer 3 g G y Y r R
p 1 action flow 0 port 0

p 1 action flow 1 meter 0 trtcm 1250000000 1250000000 1000000 1000000
p 1 action flow 1 policer 0 g G y Y r R
p 1 action flow 1 meter 1 trtcm 1250000000 1250000000 1000000 1000000
p 1 action flow 1 policer 1 g G y Y r R
p 1 action flow 1 meter 2 trtcm 1250000000 1250000000 1000000 1000000
p 1 action flow 1 policer 2 g G y Y r R
p 1 action flow 1 meter 3 trtcm 1250000000 1250000000 1000000 1000000
p 1 action flow 1 policer 3 g G y Y r R
p 1 action flow 1 port 1

p 1 action flow 2 meter 0 trtcm 1250000000 1250000000 1000000 1000000
p 1 action flow 2 policer 0 g G y Y r R
p 1 action flow 2 meter 1 trtcm 1250000000 1250000000 1000000 1000000
p 1 action flow 2 policer 1 g G y Y r R
p 1 action flow 2 meter 2 trtcm 1250000000 1250000000 1000000 1000000
p 1 action flow 2 policer 2 g G y Y r R
p 1 action flow 2 meter 3 trtcm 1250000000 1250000000 1000000 1000000
p 1 action flow 2 policer 3 g G y Y r R
p 1 action flow 2 port 2

p 1 action flow 3 meter 0 trtcm 1250000000 1250000000 1000000 1000000
p 1 action flow 3 policer 0 g G y Y r R
p 1 action flow 3 meter 1 trtcm 1250000000 1250000000 1000000 1000000
p 1 action flow 3 policer 1 g G y Y r R
p 1 action flow 3 meter 2 trtcm 1250000000 1250000000 1000000 1000000
p 1 action flow 3 policer 2 g G y Y r R
p 1 action flow 3 meter 3 trtcm 1250000000 1250000000 1000000 1000000
p 1 action flow 3 policer 3 g G y Y r R
p 1 action flow 3 port 3

#p 1 action flow bulk ./config/action.txt

#p 1 action flow ls

p 1 action flow 0 stats
p 1 action flow 1 stats
p 1 action flow 2 stats
p 1 action flow 3 stats

p 1 action dscp 0 class 0 color G
p 1 action dscp 1 class 1 color G
p 1 action dscp 2 class 2 color G
p 1 action dscp 3 class 3 color G
p 1 action dscp 4 class 0 color G
p 1 action dscp 5 class 1 color G
p 1 action dscp 6 class 2 color G
p 1 action dscp 7 class 3 color G
p 1 action dscp 8 class 0 color G
p 1 action dscp 9 class 1 color G
p 1 action dscp 10 class 2 color G
p 1 action dscp 11 class 3 color G
p 1 action dscp 12 class 0 color G
p 1 action dscp 13 class 1 color G
p 1 action dscp 14 class 2 color G
p 1 action dscp 15 class 3 color G
p 1 action dscp 16 class 0 color G
p 1 action dscp 17 class 1 color G
p 1 action dscp 18 class 2 color G
p 1 action dscp 19 class 3 color G
p 1 action dscp 20 class 0 color G
p 1 action dscp 21 class 1 color G
p 1 action dscp 22 class 2 color G
p 1 action dscp 23 class 3 color G
p 1 action dscp 24 class 0 color G
p 1 action dscp 25 class 1 color G
p 1 action dscp 26 class 2 color G
p 1 action dscp 27 class 3 color G
p 1 action dscp 27 class 0 color G
p 1 action dscp 29 class 1 color G
p 1 action dscp 30 class 2 color G
p 1 action dscp 31 class 3 color G
p 1 action dscp 32 class 0 color G
p 1 action dscp 33 class 1 color G
p 1 action dscp 34 class 2 color G
p 1 action dscp 35 class 3 color G
p 1 action dscp 36 class 0 color G
p 1 action dscp 37 class 1 color G
p 1 action dscp 38 class 2 color G
p 1 action dscp 39 class 3 color G
p 1 action dscp 40 class 0 color G
p 1 action dscp 41 class 1 color G
p 1 action dscp 42 class 2 color G
p 1 action dscp 43 class 3 color G
p 1 action dscp 44 class 0 color G
p 1 action dscp 45 class 1 color G
p 1 action dscp 46 class 2 color G
p 1 action dscp 47 class 3 color G
p 1 action dscp 48 class 0 color G
p 1 action dscp 49 class 1 color G
p 1 action dscp 50 class 2 color G
p 1 action dscp 51 class 3 color G
p 1 action dscp 52 class 0 color G
p 1 action dscp 53 class 1 color G
p 1 action dscp 54 class 2 color G
p 1 action dscp 55 class 3 color G
p 1 action dscp 56 class 0 color G
p 1 action dscp 57 class 1 color G
p 1 action dscp 58 class 2 color G
p 1 action dscp 59 class 3 color G
p 1 action dscp 60 class 0 color G
p 1 action dscp 61 class 1 color G
p 1 action dscp 62 class 2 color G
p 1 action dscp 63 class 3 color G

p 1 action dscp ls
