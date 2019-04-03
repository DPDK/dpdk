@include@
@@

#include <rte_string_fns.h>

@use_strlcpy depends on include@
expression src, dst, size;
@@
(
- snprintf(dst, size, "%s", src)
+ strlcpy(dst, src, size)
)
