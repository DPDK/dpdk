Design
======

Environment or Architecture-specific Sources
--------------------------------------------

In DPDK and DPDK applications, some code is specific to an architecture (i686, x86_64) or to an executive environment (bsdapp or linuxapp) and so on.
As far as is possible, all such instances of architecture or env-specific code should be provided via standard APIs in the EAL.

By convention, a file is common if it is not located in a directory indicating that it is specific.
For instance, a file located in a subdir of "x86_64" directory is specific to this architecture.
A file located in a subdir of "linuxapp" is specific to this execution environment.

.. note::

	Code in DPDK libraries and applications should be generic.
	The correct location for architecture or executive environment specific code is in the EAL.

When absolutely necessary, there are several ways to handle specific code:

* Use a ``#ifdef`` with the CONFIG option in the C code.
  This can be done when the differences are small and they can be embedded in the same C file:

.. code-block: console

   #ifdef RTE_ARCH_I686
   toto();
   #else
   titi();
   #endif

* Use the CONFIG option in the Makefile. This is done when the differences are more significant.
  In this case, the code is split into two separate files that are architecture or environment specific.  This should only apply inside the EAL library.

.. note:

	As in the linux kernel, the "CONFIG_" prefix is not used in C code.
	This is only needed in Makefiles or shell scripts.

Per Architecture Sources
~~~~~~~~~~~~~~~~~~~~~~~~

The following config options can be used:

* CONFIG_RTE_ARCH is a string that contains the name of the architecture.
* CONFIG_RTE_ARCH_I686, CONFIG_RTE_ARCH_X86_64, CONFIG_RTE_ARCH_X86_64_32 or CONFIG_RTE_ARCH_PPC_64 are defined only if we are building for those architectures.

Per Execution Environment Sources
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following config options can be used:

* CONFIG_RTE_EXEC_ENV is a string that contains the name of the executive environment.
* CONFIG_RTE_EXEC_ENV_BSDAPP or CONFIG_RTE_EXEC_ENV_LINUXAPP are defined only if we are building for this execution environment.
