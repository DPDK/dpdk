..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

.. _abi_versioning:

ABI Versioning
==============

This document details the mechanics of ABI version management in DPDK.

.. _what_is_soname:

What is a library's soname?
---------------------------

System libraries usually adopt the familiar major and minor version naming
convention, where major versions (e.g. ``librte_eal 20.x, 21.x``) are presumed
to be ABI incompatible with each other and minor versions (e.g. ``librte_eal
20.1, 20.2``) are presumed to be ABI compatible. A library's `soname
<https://en.wikipedia.org/wiki/Soname>`_. is typically used to provide backward
compatibility information about a given library, describing the lowest common
denominator ABI supported by the library. The soname or logical name for the
library, is typically comprised of the library's name and major version e.g.
``librte_eal.so.20``.

During an application's build process, a library's soname is noted as a runtime
dependency of the application. This information is then used by the `dynamic
linker <https://en.wikipedia.org/wiki/Dynamic_linker>`_ when resolving the
applications dependencies at runtime, to load a library supporting the correct
ABI version. The library loaded at runtime therefore, may be a minor revision
supporting the same major ABI version (e.g. ``librte_eal.20.2``), as the library
used to link the application (e.g ``librte_eal.20.0``).

.. _major_abi_versions:

Major ABI versions
------------------

An ABI version change to a given library, especially in core libraries such as
``librte_mbuf``, may cause an implicit ripple effect on the ABI of it's
consuming libraries, causing ABI breakages. There may however be no explicit
reason to bump a dependent library's ABI version, as there may have been no
obvious change to the dependent library's API, even though the library's ABI
compatibility will have been broken.

This interdependence of DPDK libraries, means that ABI versioning of libraries
is more manageable at a project level, with all project libraries sharing a
**single ABI version**. In addition, the need to maintain a stable ABI for some
number of releases as described in the section :doc:`abi_policy`, means
that ABI version increments need to carefully planned and managed at a project
level.

Major ABI versions are therefore declared typically aligned with an LTS release
and is then supported some number of subsequent releases, shared across all
libraries. This means that a single project level ABI version, reflected in all
individual library's soname, library filenames and associated version maps
persists over multiple releases.

.. code-block:: none

 $ head ./lib/librte_acl/rte_acl_version.map
 DPDK_20 {
        global:
 ...

 $ head ./lib/librte_eal/rte_eal_version.map
 DPDK_20 {
        global:
 ...

When an ABI change is made between major ABI versions to a given library, a new
section is added to that library's version map describing the impending new ABI
version, as described in the section :ref:`example_abi_macro_usage`. The
library's soname and filename however do not change, e.g. ``libacl.so.20``, as
ABI compatibility with the last major ABI version continues to be preserved for
that library.

.. code-block:: none

 $ head ./lib/librte_acl/rte_acl_version.map
 DPDK_20 {
        global:
 ...

 DPDK_21 {
        global:

 } DPDK_20;
 ...

 $ head ./lib/librte_eal/rte_eal_version.map
 DPDK_20 {
        global:
 ...

However when a new ABI version is declared, for example DPDK ``21``, old
depreciated functions may be safely removed at this point and the entire old
major ABI version removed, see the section :ref:`deprecating_entire_abi` on
how this may be done.

.. code-block:: none

 $ head ./lib/librte_acl/rte_acl_version.map
 DPDK_21 {
        global:
 ...

 $ head ./lib/librte_eal/rte_eal_version.map
 DPDK_21 {
        global:
 ...

At the same time, the major ABI version is changed atomically across all
libraries by incrementing the major version in the ABI_VERSION file. This is
done globally for all libraries that declare a stable ABI. For libraries marked
as EXPERIMENTAL, their major ABI version is always set to 0.

Minor ABI versions
~~~~~~~~~~~~~~~~~~

Each non-LTS release will also increment minor ABI version, to permit multiple
DPDK versions being installed alongside each other. Both stable and
experimental ABI's are versioned using the global version file that is updated
at the start of each release cycle, and are managed at the project level.

Versioning Macros
-----------------

When a symbol is exported from a library to provide an API, it also provides a
calling convention (ABI) that is embodied in its name, return type and
arguments. Occasionally that function may need to change to accommodate new
functionality or behavior. When that occurs, it is may be required to allow for
backward compatibility for a time with older binaries that are dynamically
linked to the DPDK.

To support backward compatibility the ``rte_function_versioning.h``
header file provides macros to use when updating exported functions. These
macros are used in conjunction with the ``rte_<library>_version.map`` file for
a given library to allow multiple versions of a symbol to exist in a shared
library so that older binaries need not be immediately recompiled.

The macros exported are:

* ``VERSION_SYMBOL(b, e, n)``: Creates a symbol version table entry binding
  versioned symbol ``b@DPDK_n`` to the internal function ``be``.

* ``BIND_DEFAULT_SYMBOL(b, e, n)``: Creates a symbol version entry instructing
  the linker to bind references to symbol ``b`` to the internal symbol
  ``be``.

* ``MAP_STATIC_SYMBOL(f, p)``: Declare the prototype ``f``, and map it to the
  fully qualified function ``p``, so that if a symbol becomes versioned, it
  can still be mapped back to the public symbol name.

* ``__vsym``:  Annotation to be used in a declaration of the internal symbol
  ``be`` to signal that it is being used as an implementation of a particular
  version of symbol ``b``.

.. _example_abi_macro_usage:

Examples of ABI Macro use
~~~~~~~~~~~~~~~~~~~~~~~~~

Updating a public API
_____________________

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
binaries.

The addition of a parameter to the function is ABI breaking as the function is
public, and existing application may use it in its current form. However, the
compatibility macros in DPDK allow a developer to use symbol versioning so that
multiple functions can be mapped to the same public symbol based on when an
application was linked to it. To see how this is done, we start with the
requisite libraries version map file. Initially the version map file for the acl
library looks like this

.. code-block:: none

   DPDK_20 {
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

   DPDK_20 {
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

   DPDK_21 {
        global:
        rte_acl_create;

   } DPDK_20;

The addition of the new block tells the linker that a new version node
``DPDK_21`` is available, which contains the symbol rte_acl_create, and inherits
the symbols from the DPDK_20 node. This list is directly translated into a
list of exported symbols when DPDK is compiled as a shared library.

Next, we need to specify in the code which function maps to the rte_acl_create
symbol at which versions.  First, at the site of the initial symbol definition,
we need to update the function so that it is uniquely named, and not in conflict
with the public symbol name

.. code-block:: c

 -struct rte_acl_ctx *
 -rte_acl_create(const struct rte_acl_param *param)
 +struct rte_acl_ctx * __vsym
 +rte_acl_create_v20(const struct rte_acl_param *param)
 {
        size_t sz;
        struct rte_acl_ctx *ctx;
        ...

Note that the base name of the symbol was kept intact, as this is conducive to
the macros used for versioning symbols and we have annotated the function as
``__vsym``, an implementation of a versioned symbol . That is our next step,
mapping this new symbol name to the initial symbol name at version node 20.
Immediately after the function, we add the VERSION_SYMBOL macro.

.. code-block:: c

   #include <rte_function_versioning.h>

   ...
   VERSION_SYMBOL(rte_acl_create, _v20, 20);

Remembering to also add the rte_function_versioning.h header to the requisite c
file where these changes are being made. The macro instructs the linker to
create a new symbol ``rte_acl_create@DPDK_20``, which matches the symbol created
in older builds, but now points to the above newly named function. We have now
mapped the original rte_acl_create symbol to the original function (but with a
new name).

Please see the section :ref:`Enabling versioning macros
<enabling_versioning_macros>` to enable this macro in the meson/ninja build.
Next, we need to create the new ``v21`` version of the symbol. We create a new
function name, with the ``v21`` suffix, and implement it appropriately.

.. code-block:: c

   struct rte_acl_ctx * __vsym
   rte_acl_create_v21(const struct rte_acl_param *param, int debug);
   {
        struct rte_acl_ctx *ctx = rte_acl_create_v20(param);

        ctx->debug = debug;

        return ctx;
   }

This code serves as our new API call. Its the same as our old call, but adds the
new parameter in place. Next we need to map this function to the new default
symbol ``rte_acl_create@DPDK_21``. To do this, immediately after the function,
we add the BIND_DEFAULT_SYMBOL macro.

.. code-block:: c

   #include <rte_function_versioning.h>

   ...
   BIND_DEFAULT_SYMBOL(rte_acl_create, _v21, 21);

The macro instructs the linker to create the new default symbol
``rte_acl_create@DPDK_21``, which points to the above newly named function.

We finally modify the prototype of the call in the public header file,
such that it contains both versions of the symbol and the public API.

.. code-block:: c

   struct rte_acl_ctx *
   rte_acl_create(const struct rte_acl_param *param);

   struct rte_acl_ctx * __vsym
   rte_acl_create_v20(const struct rte_acl_param *param);

   struct rte_acl_ctx * __vsym
   rte_acl_create_v21(const struct rte_acl_param *param, int debug);


And that's it, on the next shared library rebuild, there will be two versions of
rte_acl_create, an old DPDK_20 version, used by previously built applications,
and a new DPDK_21 version, used by future built applications.

.. note::

   **Before you leave**, please take care reviewing the sections on
   :ref:`mapping static symbols <mapping_static_symbols>`,
   :ref:`enabling versioning macros <enabling_versioning_macros>`,
   and :ref:`ABI deprecation <abi_deprecation>`.


.. _mapping_static_symbols:

Mapping static symbols
______________________

Now we've taken what was a public symbol, and duplicated it into two uniquely
and differently named symbols. We've then mapped each of those back to the
public symbol ``rte_acl_create`` with different version tags. This only applies
to dynamic linking, as static linking has no notion of versioning. That leaves
this code in a position of no longer having a symbol simply named
``rte_acl_create`` and a static build will fail on that missing symbol.

To correct this, we can simply map a function of our choosing back to the public
symbol in the static build with the ``MAP_STATIC_SYMBOL`` macro.  Generally the
assumption is that the most recent version of the symbol is the one you want to
map.  So, back in the C file where, immediately after ``rte_acl_create_v21`` is
defined, we add this


.. code-block:: c

   struct rte_acl_ctx * __vsym
   rte_acl_create_v21(const struct rte_acl_param *param, int debug)
   {
        ...
   }
   MAP_STATIC_SYMBOL(struct rte_acl_ctx *rte_acl_create(const struct rte_acl_param *param, int debug), rte_acl_create_v21);

That tells the compiler that, when building a static library, any calls to the
symbol ``rte_acl_create`` should be linked to ``rte_acl_create_v21``


.. _enabling_versioning_macros:

Enabling versioning macros
__________________________

Finally, we need to indicate to the :doc:`meson/ninja build system
<../prog_guide/build-sdk-meson>` to enable versioning macros when building the
library or driver. In the libraries or driver where we have added symbol
versioning, in the ``meson.build`` file we add the following

.. code-block::

   use_function_versioning = true

at the start of the head of the file. This will indicate to the tool-chain to
enable the function version macros when building. There is no corresponding
directive required for the ``make`` build system.

.. _abi_deprecation:

Deprecating part of a public API
________________________________

Lets assume that you've done the above updates, and in preparation for the next
major ABI version you decide you would like to retire the old version of the
function. After having gone through the ABI deprecation announcement process,
removal is easy. Start by removing the symbol from the requisite version map
file:

.. code-block:: none

   DPDK_20 {
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

   DPDK_21 {
        global:
        rte_acl_create;
   } DPDK_20;


Next remove the corresponding versioned export.

.. code-block:: c

 -VERSION_SYMBOL(rte_acl_create, _v20, 20);


Note that the internal function definition could also be removed, but its used
in our example by the newer version ``v21``, so we leave it in place and declare
it as static. This is a coding style choice.

.. _deprecating_entire_abi:

Deprecating an entire ABI version
_________________________________

While removing a symbol from an ABI may be useful, it is more practical to
remove an entire version node at once, as is typically done at the declaration
of a major ABI version. If a version node completely specifies an API, then
removing part of it, typically makes it incomplete. In those cases it is better
to remove the entire node.

To do this, start by modifying the version map file, such that all symbols from
the node to be removed are merged into the next node in the map.

In the case of our map above, it would transform to look as follows

.. code-block:: none

   DPDK_21 {
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

 -BIND_DEFAULT_SYMBOL(rte_acl_create, _v20, 20);
 +BIND_DEFAULT_SYMBOL(rte_acl_create, _v21, 21);

Lastly, any VERSION_SYMBOL macros that point to the old version node should be
removed, taking care to keep, where need old code in place to support newer
versions of the symbol.


Running the ABI Validator
-------------------------

The ``devtools`` directory in the DPDK source tree contains a utility program,
``validate-abi.sh``, for validating the DPDK ABI based on the Linux `ABI
Compliance Checker
<http://ispras.linuxbase.org/index.php/ABI_compliance_checker>`_.

This has a dependency on the ``abi-compliance-checker`` and ``and abi-dumper``
utilities which can be installed via a package manager. For example::

   sudo yum install abi-compliance-checker
   sudo yum install abi-dumper

The syntax of the ``validate-abi.sh`` utility is::

   ./devtools/validate-abi.sh <REV1> <REV2>

Where ``REV1`` and ``REV2`` are valid gitrevisions(7)
https://www.kernel.org/pub/software/scm/git/docs/gitrevisions.html
on the local repo.

For example::

   # Check between the previous and latest commit:
   ./devtools/validate-abi.sh HEAD~1 HEAD

   # Check on a specific compilation target:
   ./devtools/validate-abi.sh -t x86_64-native-linux-gcc HEAD~1 HEAD

   # Check between two tags:
   ./devtools/validate-abi.sh v2.0.0 v2.1.0

   # Check between git master and local topic-branch "vhost-hacking":
   ./devtools/validate-abi.sh master vhost-hacking

After the validation script completes (it can take a while since it need to
compile both tags) it will create compatibility reports in the
``./abi-check/compat_report`` directory. Listed incompatibilities can be found
as follows::

  grep -lr Incompatible abi-check/compat_reports/
