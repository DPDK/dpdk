// Replace zero-length array members with []
@@
identifier st, member, arr;
type T1, T2;
@@
struct st {
	...
	T1 member;
-	T2 arr[0];
+	T2 arr[];
};
@@
identifier st, member, arr, id;
type T1, T2;
@@
struct st {
	...
	T1 member;
-	T2 arr[0];
+	T2 arr[];
} id;
