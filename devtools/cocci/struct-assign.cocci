//
// prefer structure assignment over memcpy
//
@@
type T;
T *SRCP;
T *DSTP;
@@
(
- rte_memcpy(DSTP, SRCP, sizeof(T))
+ *DSTP = *SRCP
|
- memcpy(DSTP, SRCP, sizeof(T))
+ *DSTP = *SRCP
)
