@use_strlcpy@
identifier src, dst;
expression size;
@@
(
- snprintf(dst, size, "%s", src)
+ strlcpy(dst, src, size)
)
