ABI policy
==========
ABI versions are set at the time of major release labeling, and ABI may change
multiple times between the last labeling and the HEAD label of the git tree
without warning.

ABI versions, once released are available until such time as their
deprecation has been noted here for at least one major release cycle, after it
has been tagged.  E.g. the ABI for DPDK 2.0 is shipped, and then the decision to
remove it is made during the development of DPDK 2.1.  The decision will be
recorded here, shipped with the DPDK 2.1 release, and actually removed when DPDK
2.2 ships.

ABI versions may be deprecated in whole, or in part as needed by a given update.

Some ABI changes may be too significant to reasonably maintain multiple
versions of.  In those events ABI's may be updated without backward
compatibility provided.  The requirements for doing so are:

#. At least 3 acknoweldgements of the need on the dpdk.org
#. A full deprecation cycle must be made to offer downstream consumers sufficient warning of the change.  E.g. if dpdk 2.0 is under development when the change is proposed, a deprecation notice must be added to this file, and released with dpdk 2.0.  Then the change may be incorporated for dpdk 2.1
#. The LIBABIVER variable in the makefile(s) where the ABI changes are incorporated must be incremented in parallel with the ABI changes themselves

Note that the above process for ABI deprecation should not be undertaken
lightly.  ABI stability is extremely important for downstream consumers of the
DPDK, especially when distributed in shared object form.  Every effort should be
made to preserve ABI whenever possible.  For instance, reorganizing public
structure field for astetic or readability purposes should be avoided as it will
cause ABI breakage.  Only significant (e.g. performance) reasons should be seen
as cause to alter ABI.

Examples of Deprecation Notices
-------------------------------
* The Macro #RTE_FOO is deprecated and will be removed with version 2.0, to be replaced with the inline function rte_bar()
* The function rte_mbuf_grok has been updated to include new parameter in version 2.0.  Backwards compatibility will be maintained for this function until the release of version 2.1
* The members struct foo have been reorganized in release 2.0.  Existing binary applications will have backwards compatibility in release 2.0, while newly built binaries will need to reference new structure variant struct foo2.  Compatibility will be removed in release 2.2, and all applications will require updating and rebuilding to the new structure at that time, which will be renamed to the original struct foo.
* Significant ABI changes are planned for the librte_dostuff library.  The upcoming release 2.0 will not contain these changes, but release 2.1 will, and no backwards compatibility is planned due to the invasive nature of these changes.  Binaries using this library built prior to version 2.1 will require updating and recompilation.

Deprecation Notices
-------------------
