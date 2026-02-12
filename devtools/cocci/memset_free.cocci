// Replace calls to memset before free
@@
expression E, size;
@@
(
- memset(E, 0, size);
- rte_free(E);
+ rte_free_sensitive(E);
)
