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
convention, where major versions (e.g. ``librte_eal 21.x, 22.x``) are presumed
to be ABI incompatible with each other and minor versions (e.g. ``librte_eal
21.1, 21.2``) are presumed to be ABI compatible. A library's `soname
<https://en.wikipedia.org/wiki/Soname>`_. is typically used to provide backward
compatibility information about a given library, describing the lowest common
denominator ABI supported by the library. The soname or logical name for the
library, is typically comprised of the library's name and major version e.g.
``librte_eal.so.21``.

During an application's build process, a library's soname is noted as a runtime
dependency of the application. This information is then used by the `dynamic
linker <https://en.wikipedia.org/wiki/Dynamic_linker>`_ when resolving the
applications dependencies at runtime, to load a library supporting the correct
ABI version. The library loaded at runtime therefore, may be a minor revision
supporting the same major ABI version (e.g. ``librte_eal.21.2``), as the library
used to link the application (e.g ``librte_eal.21.0``).

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

 $ head ./build/lib/acl_exports.map
 DPDK_21 {
        global:
 ...

 $ head ./build/lib/eal_exports.map
 DPDK_21 {
        global:
 ...

When an ABI change is made between major ABI versions to a given library, a new
section is added to that library's version map describing the impending new ABI
version, as described in the section :ref:`example_abi_macro_usage`. The
library's soname and filename however do not change, e.g. ``libacl.so.21``, as
ABI compatibility with the last major ABI version continues to be preserved for
that library.

.. code-block:: none

 $ head ./build/lib/acl_exports.map
 DPDK_21 {
        global:
 ...

 DPDK_22 {
        global:

 } DPDK_21;
 ...

 $ head ./build/lib/eal_exports.map
 DPDK_21 {
        global:
 ...

However when a new ABI version is declared, for example DPDK ``22``, old
deprecated functions may be safely removed at this point and the entire old
major ABI version removed, see the section :ref:`deprecating_entire_abi` on
how this may be done.

.. code-block:: none

 $ head ./build/lib/acl_exports.map
 DPDK_22 {
        global:
 ...

 $ head ./build/lib/eal_exports.map
 DPDK_22 {
        global:
 ...

At the same time, the major ABI version is changed atomically across all
libraries by incrementing the major version in the ABI_VERSION file. This is
done globally for all libraries.

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

To support backward compatibility the ``eal_export.h``
header file provides macros to use when updating exported functions. These
macros allow multiple versions of a symbol to exist in a shared
library so that older binaries need not be immediately recompiled.

The macros are:

* ``RTE_VERSION_SYMBOL(ver, type, name, args)``: Creates a symbol version table
  entry binding symbol ``<name>@DPDK_<ver>`` to the internal function name
  ``<name>_v<ver>``.

* ``RTE_DEFAULT_SYMBOL(ver, type, name, args)``: Creates a symbol version entry
  instructing the linker to bind references to symbol ``<name>`` to the internal
  symbol ``<name>_v<ver>``.

* ``RTE_VERSION_EXPERIMENTAL_SYMBOL(type, name, args)``:  Similar to RTE_VERSION_SYMBOL
  but for experimental API symbols. The macro is used when a symbol matures
  to become part of the stable ABI, to provide an alias to experimental
  until the next major ABI version.

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
 RTE_EXPORT_SYMBOL(rte_acl_create)
 int
 rte_acl_create(struct rte_acl_param *param)
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
 RTE_EXPORT_SYMBOL(rte_acl_create)
 int
 rte_acl_create(struct rte_acl_param *param, int debug)
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
application was linked to it.

We need to specify in the code which function maps to the rte_acl_create
symbol at which versions.  First, at the site of the initial symbol definition,
we wrap the function with ``RTE_VERSION_SYMBOL``, passing the current ABI version,
the function return type, the function name and its arguments.

.. code-block:: c

 -RTE_EXPORT_SYMBOL(rte_acl_create)
 -int
 -rte_acl_create(struct rte_acl_param *param)
 +RTE_VERSION_SYMBOL(21, int, rte_acl_create, (struct rte_acl_param *param))
 {
        size_t sz;
        struct rte_acl_ctx *ctx;
        ...

The macro instructs the linker to create a new symbol ``rte_acl_create@DPDK_21``,
which matches the symbol created in older builds,
but now points to the above newly named function ``rte_acl_create_v21``.
We have now mapped the original rte_acl_create symbol to the original function
(but with a new name).

Please see the section :ref:`Enabling versioning macros
<enabling_versioning_macros>` to enable this macro in the meson/ninja build.

Next, we need to create the new version of the symbol. We create a new
function name and implement it appropriately, then wrap it in a call to ``RTE_DEFAULT_SYMBOL``.

.. code-block:: c

   RTE_DEFAULT_SYMBOL(22, int, rte_acl_create, (struct rte_acl_param *param, int debug))
   {
        int ret = rte_acl_create_v21(param);

        if (debug) {
        ...
        }

        return ret;
   }

The macro instructs the linker to create the new default symbol
``rte_acl_create@DPDK_22``, which points to the function named ``rte_acl_create_v22``
(declared by the macro).

And that's it. On the next shared library rebuild, there will be two versions of rte_acl_create,
an old DPDK_21 version, used by previously built applications, and a new DPDK_22 version,
used by newly built applications.

.. note::

   **Before you leave**, please take care reviewing the sections on
   :ref:`enabling versioning macros <enabling_versioning_macros>`,
   and :ref:`ABI deprecation <abi_deprecation>`.


.. _enabling_versioning_macros:

Enabling versioning macros
__________________________

Finally, we need to indicate to the :doc:`meson/ninja build system
<../prog_guide/build-sdk-meson>` to enable versioning macros when building the
library or driver. In the libraries or driver where we have added symbol
versioning, in the ``meson.build`` file we add the following

.. code-block:: none

   use_function_versioning = true

at the start of the head of the file. This will indicate to the tool-chain to
enable the function version macros when building.


.. _aliasing_experimental_symbols:

Aliasing experimental symbols
_____________________________

In situations in which an ``experimental`` symbol has been stable for some time,
and it becomes a candidate for promotion to the stable ABI. At this time, when
promoting the symbol, the maintainer may choose to provide an alias to the
``experimental`` symbol version, so as not to break consuming applications.
This alias is then dropped in the next major ABI version.

The process to provide an alias to ``experimental`` is similar to that, of
:ref:`symbol versioning <example_abi_macro_usage>` described above.
Assume we have an experimental function ``rte_acl_create`` as follows:

.. code-block:: c

   #include <rte_compat.h>

   /*
    * Create an acl context object for apps to
    * manipulate
    */
   RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_acl_create)
   __rte_experimental
   int
   rte_acl_create(struct rte_acl_param *param)
   {
   ...
   }

When we promote the symbol to the stable ABI, we simply strip the
``__rte_experimental`` annotation from the function.

.. code-block:: c

   /*
    * Create an acl context object for apps to
    * manipulate
    */
   RTE_EXPORT_SYMBOL(rte_acl_create)
   int
   rte_acl_create(struct rte_acl_param *param)
   {
          ...
   }

Although there are strictly no guarantees or commitments associated with
:ref:`experimental symbols <experimental_apis>`, a maintainer may wish to offer
an alias to experimental. The process to add an alias to experimental,
is similar to the symbol versioning process. Assuming we have an experimental
symbol as before, we now add the symbol to both the ``EXPERIMENTAL``
and ``DPDK_22`` version nodes.

.. code-block:: c

   #include <rte_compat.h>;

   /*
    * Create an acl context object for apps to
    * manipulate
    */
   RTE_DEFAULT_SYMBOL(22, int, rte_acl_create, (struct rte_acl_param *param))
   {
   ...
   }

   RTE_VERSION_EXPERIMENTAL_SYMBOL(int, rte_acl_create, (struct rte_acl_param *param))
   {
      return rte_acl_create(param);
   }

.. _abi_deprecation:

Deprecating part of a public API
________________________________

Lets assume that you've done the above updates, and in preparation for the next
major ABI version you decide you would like to retire the old version of the
function. After having gone through the ABI deprecation announcement process,
removal is easy.

Next remove the corresponding versioned export.

.. code-block:: c

 -RTE_VERSION_SYMBOL(21, int, rte_acl_create, (struct rte_acl_param *param))


Note that the internal function definition must also be removed, but it is used
in our example by the newer version ``v22``, so we leave it in place and declare
it as static. This is a coding style choice.

.. _deprecating_entire_abi:

Deprecating an entire ABI version
_________________________________

While removing a symbol from an ABI may be useful, it is more practical to
remove an entire version node at once, as is typically done at the declaration
of a major ABI version. If a version node completely specifies an API, then
removing part of it, typically makes it incomplete. In those cases it is better
to remove the entire node.

Any uses of RTE_DEFAULT_SYMBOL that pointed to the old node should be
updated to point to the new version node in any header files for all affected
symbols.

.. code-block:: c

 -RTE_DEFAULT_SYMBOL(21, int, rte_acl_create, (struct rte_acl_param *param, int debug))
 +RTE_DEFAULT_SYMBOL(22, int, rte_acl_create, (struct rte_acl_param *param, int debug))

Lastly, any RTE_VERSION_SYMBOL macros that point to the old version nodes
should be removed, taking care to preserve any code that is shared
with the new version node.


Running the ABI Validator
-------------------------

The ``devtools`` directory in the DPDK source tree contains a utility program,
``check-abi.sh``, for validating the DPDK ABI based on the libabigail
`abidiff utility <https://sourceware.org/libabigail/manual/abidiff.html>`_.

The syntax of the ``check-abi.sh`` utility is::

   devtools/check-abi.sh <refdir> <newdir>

Where <refdir> specifies the directory housing the reference build of DPDK,
and <newdir> specifies the DPDK build directory to check the ABI of.

The ABI compatibility is automatically verified when using a build script
from ``devtools``, if the variable ``DPDK_ABI_REF_VERSION`` is set with a tag,
as described in :ref:`ABI check recommendations<integrated_abi_check>`.
