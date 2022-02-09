//
// Remove unnecessary NULL pointer checks before free functions
// All these functions work like libc free which allows
// free(NULL) as a no-op.
//
@@
expression E;
@@
(
- if (E != NULL) free(E);
+ free(E);
|
- if (E != NULL) rte_bitmap_free(E);
+ rte_bitmap_free(E);
|
- if (E != NULL) rte_free(E);
+ rte_free(E);
|
- if (E != NULL) rte_hash_free(E);
+ rte_hash_free(E);
|
- if (E != NULL) rte_ring_free(E);
+ rte_ring_free(E);
|
- if (E != NULL) rte_pktmbuf_free(E);
+ rte_pktmbuf_free(E);
|
- if (E != NULL) rte_mempool_free(E);
+ rte_mempool_free(E);
|
- if (E != NULL) rte_kvargs_free(E);
+ rte_kvargs_free(E);
)
