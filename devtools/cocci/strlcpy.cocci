@use_strlcpy@
expression src, dst, size;
@@
(
- snprintf(dst, size, "%s", src)
+ strlcpy(dst, src, size)
)
