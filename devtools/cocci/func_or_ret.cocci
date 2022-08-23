@@
expression cond, ret;
@@
-RTE_FUNC_PTR_OR_ERR_RET(cond, ret);
+if (cond == NULL)
+	return ret;
@@
expression cond;
@@
-RTE_FUNC_PTR_OR_RET(cond);
+if (cond == NULL)
+	return;
