//
// Replace explicit packet offset computations with rte_pktmbuf_mtod_offset().
//
@disable paren@
typedef uint8_t;
expression M, O;
@@
(
- rte_pktmbuf_mtod(M, char *) + O
+ rte_pktmbuf_mtod_offset(M, char *, O)
|
- rte_pktmbuf_mtod(M, char *) - O
+ rte_pktmbuf_mtod_offset(M, char *, -O)
|
- rte_pktmbuf_mtod(M, unsigned char *) + O
+ rte_pktmbuf_mtod_offset(M, unsigned char *, O)
|
- rte_pktmbuf_mtod(M, unsigned char *) - O
+ rte_pktmbuf_mtod_offset(M, unsigned char *, -O)
|
- rte_pktmbuf_mtod(M, uint8_t *) + O
+ rte_pktmbuf_mtod_offset(M, uint8_t *, O)
|
- rte_pktmbuf_mtod(M, uint8_t *) - O
+ rte_pktmbuf_mtod_offset(M, uint8_t *, -O)
)


//
// Fold subsequent offset terms into pre-existing offset used in
// rte_pktmbuf_mtod_offset().
//
@disable paren@
expression M, O1, O2;
@@
(
- rte_pktmbuf_mtod_offset(M, char *, O1) + O2
+ rte_pktmbuf_mtod_offset(M, char *, O1 + O2)
|
- rte_pktmbuf_mtod_offset(M, char *, O1) - O2
+ rte_pktmbuf_mtod_offset(M, char *, O1 - O2)
|
- rte_pktmbuf_mtod_offset(M, unsigned char *, O1) + O2
+ rte_pktmbuf_mtod_offset(M, unsigned char *, O1 + O2)
|
- rte_pktmbuf_mtod_offset(M, unsigned char *, O1) - O2
+ rte_pktmbuf_mtod_offset(M, unsigned char *, O1 - O2)
|
- rte_pktmbuf_mtod_offset(M, uint8_t *, O1) + O2
+ rte_pktmbuf_mtod_offset(M, uint8_t *, O1 + O2)
|
- rte_pktmbuf_mtod_offset(M, uint8_t *, O1) - O2
+ rte_pktmbuf_mtod_offset(M, uint8_t *, O1 - O2)
)


//
// Cleanup rules.  Fold in double casts, remove unnecessary paranthesis, etc.
//
@disable paren@
expression M, O;
type C, T;
@@
(
- (C)rte_pktmbuf_mtod_offset(M, T, O)
+ rte_pktmbuf_mtod_offset(M, C, O)
|
- (rte_pktmbuf_mtod_offset(M, T, O))
+ rte_pktmbuf_mtod_offset(M, T, O)
|
- (C)rte_pktmbuf_mtod(M, T)
+ rte_pktmbuf_mtod(M, C)
|
- (rte_pktmbuf_mtod(M, T))
+ rte_pktmbuf_mtod(M, T)
)
