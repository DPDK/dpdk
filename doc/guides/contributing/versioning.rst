Managing ABI updates
====================

Description
-----------

This document details some methods for handling ABI management in the DPDK.
Note this document is not exhaustive, in that C library versioning is flexible
allowing multiple methods to achieve various goals, but it will provide the user
with some introductory methods

General Guidelines
------------------

#. Whenever possible, ABI should be preserved
#. The libraries marked in experimental state may change without constraint.
#. The addition of symbols is generally not problematic
#. The modification of symbols can generally be managed with versioning
#. The removal of symbols generally is an ABI break and requires bumping of the
   LIBABIVER macro

What is an ABI
--------------

An ABI (Application Binary Interface) is the set of runtime interfaces exposed
by a library. It is similar to an API (Application Programming Interface) but
is the result of compilation.  It is also effectively cloned when applications
link to dynamic libraries.  That is to say when an application is compiled to
link against dynamic libraries, it is assumed that the ABI remains constant
between the time the application is compiled/linked, and the time that it runs.
Therefore, in the case of dynamic linking, it is critical that an ABI is
preserved, or (when modified), done in such a way that the application is unable
to behave improperly or in an unexpected fashion.

The DPDK ABI policy
-------------------

ABI versions are set at the time of major release labeling, and the ABI may
change multiple times, without warning, between the last release label and the
HEAD label of the git tree.

ABI versions, once released, are available until such time as their
deprecation has been noted in the Release Notes for at least one major release
cycle. For example consider the case where the ABI for DPDK 2.0 has been
shipped and then a decision is made to modify it during the development of
DPDK 2.1. The decision will be recorded in the Release Notes for the DPDK 2.1
release and the modification will be made available in the DPDK 2.2 release.

ABI versions may be deprecated in whole or in part as needed by a given
update.

Some ABI changes may be too significant to reasonably maintain multiple
versions. In those cases ABI's may be updated without backward compatibility
being provided. The requirements for doing so are:

#. At least 3 acknowledgments of the need to do so must be made on the
   dpdk.org mailing list.

#. The changes (including an alternative map file) must be gated with
   the ``RTE_NEXT_ABI`` option, and provided with a deprecation notice at the
   same time.
   It will become the default ABI in the next release.

#. A full deprecation cycle, as explained above, must be made to offer
   downstream consumers sufficient warning of the change.

#. At the beginning of the next release cycle, every ``RTE_NEXT_ABI``
   conditions will be removed, the ``LIBABIVER`` variable in the makefile(s)
   where the ABI is changed will be incremented, and the map files will
   be updated.

Note that the above process for ABI deprecation should not be undertaken
lightly. ABI stability is extremely important for downstream consumers of the
DPDK, especially when distributed in shared object form. Every effort should
be made to preserve the ABI whenever possible. The ABI should only be changed
for significant reasons, such as performance enhancements. ABI breakage due to
changes such as reorganizing public structure fields for aesthetic or
readability purposes should be avoided.

Examples of Deprecation Notices
-------------------------------

The following are some examples of ABI deprecation notices which would be
added to the Release Notes:

* The Macro ``#RTE_FOO`` is deprecated and will be removed with version 2.0,
  to be replaced with the inline function ``rte_foo()``.

* The function ``rte_mbuf_grok()`` has been updated to include a new parameter
  in version 2.0. Backwards compatibility will be maintained for this function
  until the release of version 2.1

* The members of ``struct rte_foo`` have been reorganized in release 2.0 for
  performance reasons. Existing binary applications will have backwards
  compatibility in release 2.0, while newly built binaries will need to
  reference the new structure variant ``struct rte_foo2``. Compatibility will
  be removed in release 2.2, and all applications will require updating and
  rebuilding to the new structure at that time, which will be renamed to the
  original ``struct rte_foo``.

* Significant ABI changes are planned for the ``librte_dostuff`` library. The
  upcoming release 2.0 will not contain these changes, but release 2.1 will,
  and no backwards compatibility is planned due to the extensive nature of
  these changes. Binaries using this library built prior to version 2.1 will
  require updating and recompilation.

Versioning Macros
-----------------

When a symbol is exported from a library to provide an API, it also provides a
calling convention (ABI) that is embodied in its name, return type and
arguments. Occasionally that function may need to change to accommodate new
functionality or behavior. When that occurs, it is desirable to allow for
backward compatibility for a time with older binaries that are dynamically
linked to the DPDK.

To support backward compatibility the ``lib/librte_compat/rte_compat.h``
header file provides macros to use when updating exported functions. These
macros are used in conjunction with the ``rte_<library>_version.map`` file for
a given library to allow multiple versions of a symbol to exist in a shared
library so that older binaries need not be immediately recompiled.

The macros exported are:

* ``VERSION_SYMBOL(b, e, n)``: Creates a symbol version table entry binding
  versioned symbol ``b@DPDK_n`` to the internal function ``b_e``.

* ``BIND_DEFAULT_SYMBOL(b, e, n)``: Creates a symbol version entry instructing
  the linker to bind references to symbol ``b`` to the internal symbol
  ``b_e``.

* ``MAP_STATIC_SYMBOL(f, p)``: Declare the prototype ``f``, and map it to the
  fully qualified function ``p``, so that if a symbol becomes versioned, it
  can still be mapped back to the public symbol name.

Examples of ABI Macro use
-------------------------

Updating a public API
~~~~~~~~~~~~~~~~~~~~~

Assume we have a function as follows

.. code-block:: c

 /*
  * Create an acl context object for apps to
  * manipulate
  */
 struct rte_acl_ctx *
 rte_acl_create(const struct rte_acl_param *param)
 {
        ...
 }


Assume that struct rte_acl_ctx is a private structure, and that a developer
wishes to enhance the acl api so that a debugging flag can be enabled on a
per-context basis.  This requires an addition to the structure (which, being
private, is safe), but it also requires modifying the code as follows

.. code-block:: c

 /*
  * Create an acl context object for apps to
  * manipulate
  */
 struct rte_acl_ctx *
 rte_acl_create(const struct rte_acl_param *param, int debug)
 {
        ...
 }


Note also that, being a public function, the header file prototype must also be
changed, as must all the call sites, to reflect the new ABI footprint.  We will
maintain previous ABI versions that are accessible only to previously compiled
binaries

The addition of a parameter to the function is ABI breaking as the function is
public, and existing application may use it in its current form.  However, the
compatibility macros in DPDK allow a developer to use symbol versioning so that
multiple functions can be mapped to the same public symbol based on when an
application was linked to it.  To see how this is done, we start with the
requisite libraries version map file.  Initially the version map file for the
acl library looks like this

.. code-block:: none

   DPDK_2.0 {
        global:

        rte_acl_add_rules;
        rte_acl_build;
        rte_acl_classify;
        rte_acl_classify_alg;
        rte_acl_classify_scalar;
        rte_acl_create;
        rte_acl_dump;
        rte_acl_find_existing;
        rte_acl_free;
        rte_acl_ipv4vlan_add_rules;
        rte_acl_ipv4vlan_build;
        rte_acl_list_dump;
        rte_acl_reset;
        rte_acl_reset_rules;
        rte_acl_set_ctx_classify;

        local: *;
   };

This file needs to be modified as follows

.. code-block:: none

   DPDK_2.0 {
        global:

        rte_acl_add_rules;
        rte_acl_build;
        rte_acl_classify;
        rte_acl_classify_alg;
        rte_acl_classify_scalar;
        rte_acl_create;
        rte_acl_dump;
        rte_acl_find_existing;
        rte_acl_free;
        rte_acl_ipv4vlan_add_rules;
        rte_acl_ipv4vlan_build;
        rte_acl_list_dump;
        rte_acl_reset;
        rte_acl_reset_rules;
        rte_acl_set_ctx_classify;

        local: *;
   };

   DPDK_2.1 {
        global:
        rte_acl_create;

   } DPDK_2.0;

The addition of the new block tells the linker that a new version node is
available (DPDK_2.1), which contains the symbol rte_acl_create, and inherits the
symbols from the DPDK_2.0 node.  This list is directly translated into a list of
exported symbols when DPDK is compiled as a shared library

Next, we need to specify in the code which function map to the rte_acl_create
symbol at which versions.  First, at the site of the initial symbol definition,
we need to update the function so that it is uniquely named, and not in conflict
with the public symbol name

.. code-block:: c

  struct rte_acl_ctx *
 -rte_acl_create(const struct rte_acl_param *param)
 +rte_acl_create_v20(const struct rte_acl_param *param)
 {
        size_t sz;
        struct rte_acl_ctx *ctx;
        ...

Note that the base name of the symbol was kept intact, as this is conducive to
the macros used for versioning symbols.  That is our next step, mapping this new
symbol name to the initial symbol name at version node 2.0.  Immediately after
the function, we add this line of code

.. code-block:: c

   VERSION_SYMBOL(rte_acl_create, _v20, 2.0);

Remembering to also add the rte_compat.h header to the requisite c file where
these changes are being made.  The above macro instructs the linker to create a
new symbol ``rte_acl_create@DPDK_2.0``, which matches the symbol created in older
builds, but now points to the above newly named function.  We have now mapped
the original rte_acl_create symbol to the original function (but with a new
name)

Next, we need to create the 2.1 version of the symbol.  We create a new function
name, with a different suffix, and  implement it appropriately

.. code-block:: c

   struct rte_acl_ctx *
   rte_acl_create_v21(const struct rte_acl_param *param, int debug);
   {
        struct rte_acl_ctx *ctx = rte_acl_create_v20(param);

        ctx->debug = debug;

        return ctx;
   }

This code serves as our new API call.  Its the same as our old call, but adds
the new parameter in place.  Next we need to map this function to the symbol
``rte_acl_create@DPDK_2.1``.  To do this, we modify the public prototype of the call
in the header file, adding the macro there to inform all including applications,
that on re-link, the default rte_acl_create symbol should point to this
function.  Note that we could do this by simply naming the function above
rte_acl_create, and the linker would chose the most recent version tag to apply
in the version script, but we can also do this in the header file

.. code-block:: c

   struct rte_acl_ctx *
   -rte_acl_create(const struct rte_acl_param *param);
   +rte_acl_create(const struct rte_acl_param *param, int debug);
   +BIND_DEFAULT_SYMBOL(rte_acl_create, _v21, 2.1);

The BIND_DEFAULT_SYMBOL macro explicitly tells applications that include this
header, to link to the rte_acl_create_v21 function and apply the DPDK_2.1
version node to it.  This method is more explicit and flexible than just
re-implementing the exact symbol name, and allows for other features (such as
linking to the old symbol version by default, when the new ABI is to be opt-in
for a period.

One last thing we need to do.  Note that we've taken what was a public symbol,
and duplicated it into two uniquely and differently named symbols.  We've then
mapped each of those back to the public symbol ``rte_acl_create`` with different
version tags.  This only applies to dynamic linking, as static linking has no
notion of versioning.  That leaves this code in a position of no longer having a
symbol simply named ``rte_acl_create`` and a static build will fail on that
missing symbol.

To correct this, we can simply map a function of our choosing back to the public
symbol in the static build with the ``MAP_STATIC_SYMBOL`` macro.  Generally the
assumption is that the most recent version of the symbol is the one you want to
map.  So, back in the C file where, immediately after ``rte_acl_create_v21`` is
defined, we add this

.. code-block:: c

   struct rte_acl_create_v21(const struct rte_acl_param *param, int debug)
   {
        ...
   }
   MAP_STATIC_SYMBOL(struct rte_acl_create(const struct rte_acl_param *param, int debug), rte_acl_create_v21);

That tells the compiler that, when building a static library, any calls to the
symbol ``rte_acl_create`` should be linked to ``rte_acl_create_v21``

That's it, on the next shared library rebuild, there will be two versions of
rte_acl_create, an old DPDK_2.0 version, used by previously built applications,
and a new DPDK_2.1 version, used by future built applications.


Deprecating part of a public API
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Lets assume that you've done the above update, and after a few releases have
passed you decide you would like to retire the old version of the function.
After having gone through the ABI deprecation announcement process, removal is
easy.  Start by removing the symbol from the requisite version map file:

.. code-block:: none

   DPDK_2.0 {
        global:

        rte_acl_add_rules;
        rte_acl_build;
        rte_acl_classify;
        rte_acl_classify_alg;
        rte_acl_classify_scalar;
        rte_acl_dump;
 -      rte_acl_create
        rte_acl_find_existing;
        rte_acl_free;
        rte_acl_ipv4vlan_add_rules;
        rte_acl_ipv4vlan_build;
        rte_acl_list_dump;
        rte_acl_reset;
        rte_acl_reset_rules;
        rte_acl_set_ctx_classify;

        local: *;
   };

   DPDK_2.1 {
        global:
        rte_acl_create;
   } DPDK_2.0;


Next remove the corresponding versioned export.

.. code-block:: c

 -VERSION_SYMBOL(rte_acl_create, _v20, 2.0);


Note that the internal function definition could also be removed, but its used
in our example by the newer version _v21, so we leave it in place.  This is a
coding style choice.

Lastly, we need to bump the LIBABIVER number for this library in the Makefile to
indicate to applications doing dynamic linking that this is a later, and
possibly incompatible library version:

.. code-block:: c

   -LIBABIVER := 1
   +LIBABIVER := 2

Deprecating an entire ABI version
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

While removing a symbol from and ABI may be useful, it is often more practical
to remove an entire version node at once.  If a version node completely
specifies an API, then removing part of it, typically makes it incomplete.  In
those cases it is better to remove the entire node

To do this, start by modifying the version map file, such that all symbols from
the node to be removed are merged into the next node in the map

In the case of our map above, it would transform to look as follows

.. code-block:: none

   DPDK_2.1 {
        global:

        rte_acl_add_rules;
        rte_acl_build;
        rte_acl_classify;
        rte_acl_classify_alg;
        rte_acl_classify_scalar;
        rte_acl_dump;
        rte_acl_create
        rte_acl_find_existing;
        rte_acl_free;
        rte_acl_ipv4vlan_add_rules;
        rte_acl_ipv4vlan_build;
        rte_acl_list_dump;
        rte_acl_reset;
        rte_acl_reset_rules;
        rte_acl_set_ctx_classify;

        local: *;
 };

Then any uses of BIND_DEFAULT_SYMBOL that pointed to the old node should be
updated to point to the new version node in any header files for all affected
symbols.

.. code-block:: c

 -BIND_DEFAULT_SYMBOL(rte_acl_create, _v20, 2.0);
 +BIND_DEFAULT_SYMBOL(rte_acl_create, _v21, 2.1);

Lastly, any VERSION_SYMBOL macros that point to the old version node should be
removed, taking care to keep, where need old code in place to support newer
versions of the symbol.

Running the ABI Validator
-------------------------

The ``scripts`` directory in the DPDK source tree contains a utility program,
``validate-abi.sh``, for validating the DPDK ABI based on the Linux `ABI
Compliance Checker
<http://ispras.linuxbase.org/index.php/ABI_compliance_checker>`_.

This has a dependency on the ``abi-compliance-checker`` and ``and abi-dumper``
utilities which can be installed via a package manager. For example::

   sudo yum install abi-compliance-checker
   sudo yum install abi-dumper

The syntax of the ``validate-abi.sh`` utility is::

   ./scripts/validate-abi.sh <REV1> <REV2> <TARGET>

Where ``REV1`` and ``REV2`` are valid gitrevisions(7)
https://www.kernel.org/pub/software/scm/git/docs/gitrevisions.html
on the local repo and target is the usual DPDK compilation target.

For example::

   # Check between the previous and latest commit:
   ./scripts/validate-abi.sh HEAD~1 HEAD x86_64-native-linuxapp-gcc

   # Check between two tags:
   ./scripts/validate-abi.sh v2.0.0 v2.1.0 x86_64-native-linuxapp-gcc

   # Check between git master and local topic-branch "vhost-hacking":
   ./scripts/validate-abi.sh master vhost-hacking x86_64-native-linuxapp-gcc

After the validation script completes (it can take a while since it need to
compile both tags) it will create compatibility reports in the
``./compat_report`` directory. Listed incompatibilities can be found as
follows::

  grep -lr Incompatible compat_reports/
