ABI policy
==========
See the :doc:`guidelines document for details of the ABI policy </guidelines/versioning>`.
ABI deprecation notices are to be posted here.

Examples of Deprecation Notices
-------------------------------
* The Macro #RTE_FOO is deprecated and will be removed with version 2.0, to be replaced with the inline function rte_bar()
* The function rte_mbuf_grok has been updated to include new parameter in version 2.0.  Backwards compatibility will be maintained for this function until the release of version 2.1
* The members struct foo have been reorganized in release 2.0.  Existing binary applications will have backwards compatibility in release 2.0, while newly built binaries will need to reference new structure variant struct foo2.  Compatibility will be removed in release 2.2, and all applications will require updating and rebuilding to the new structure at that time, which will be renamed to the original struct foo.
* Significant ABI changes are planned for the librte_dostuff library.  The upcoming release 2.0 will not contain these changes, but release 2.1 will, and no backwards compatibility is planned due to the invasive nature of these changes.  Binaries using this library built prior to version 2.1 will require updating and recompilation.

Deprecation Notices
-------------------
