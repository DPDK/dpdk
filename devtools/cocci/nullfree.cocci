//
// Remove unnecessary NULL pointer checks before free functions
// All these functions work like libc free which allows
// free(NULL) as a no-op.
//
@@
expression E;
@@
(
- if (E != NULL) cmdline_free(E);
+ cmdline_free(E);
|
- if (E != NULL) free(E);
+ free(E);
|
- if (E != NULL) rte_acl_free(E);
+ rte_acl_free(E);
|
- if (E != NULL) rte_bitmap_free(E);
+ rte_bitmap_free(E);
|
- if (E != NULL) rte_comp_op_free(E);
+ rte_comp_op_free(E);
|
- if (E != NULL) rte_crypto_op_free(E);
+ rte_crypto_op_free(E);
|
- if (E != NULL) rte_efd_free(E);
+ rte_efd_free(E);
|
- if (E != NULL) rte_event_ring_free(E);
+ rte_event_ring_free(E);
|
- if (E != NULL) rte_fib_free(E);
+ rte_fib_free(E);
|
- if (E != NULL) rte_fib6_free(E);
+ rte_fib6_free(E);
|
- if (E != NULL) rte_flow_classifier_free(E);
+ rte_flow_classifier_free(E);
|
- if (E != NULL) rte_free(E);
+ rte_free(E);
|
- if (E != NULL) rte_fbk_hash_free(E);
+ rte_fbk_hash_free(E);
|
- if (E != NULL) rte_gpu_mem_free(E);
+ rte_gpu_mem_free(E);
|
- if (E != NULL) rte_hash_free(E);
+ rte_hash_free(E);
|
- if (E != NULL) rte_intr_instance_free(E);
+ rte_intr_instance_free(E);
|
- if (E != NULL) rte_intr_vec_list_free(E);
+ rte_intr_vec_list_free(E);
|
- if (E != NULL) rte_kvargs_free(E);
+ rte_kvargs_free(E);
|
- if (E != NULL) rte_lpm_free(E);
+ rte_lpm_free(E);
|
- if (E != NULL) rte_lpm6_free(E);
+ rte_lpm6_free(E);
|
- if (E != NULL) rte_member_free(E);
+ rte_member_free(E);
|
- if (E != NULL) rte_mempool_free(E);
+ rte_mempool_free(E);
|
- if (E != NULL) rte_pktmbuf_free(E);
+ rte_pktmbuf_free(E);
|
- if (E != NULL) rte_rib_free(E);
+ rte_rib_free(E);
|
- if (E != NULL) rte_rib6_free(E);
+ rte_rib6_free(E);
|
- if (E != NULL) rte_reorder_free(E);
+ rte_reorder_free(E);
|
- if (E != NULL) rte_ring_free(E);
+ rte_ring_free(E);
|
- if (E != NULL) rte_port_in_action_free(E);
+ rte_port_in_action_free(E);
|
- if (E != NULL) rte_port_in_action_profile_free(E);
+ rte_port_in_action_profile_free(E);
|
- if (E != NULL) rte_sched_port_free(E);
+ rte_sched_port_free(E);
|
- if (E != NULL) rte_stack_free(E);
+ rte_stack_free(E);
|
- if (E != NULL) rte_stats_bitrate_free(E);
+ rte_stats_bitrate_free(E);
|
- if (E != NULL) rte_swx_ctl_pipeline_free(E);
+ rte_swx_ctl_pipeline_free(E);
|
- if (E != NULL) rte_swx_pipeline_free(E);
+ rte_swx_pipeline_free(E);
|
- if (E != NULL) rte_swx_table_learner_free(E);
+ rte_swx_table_learner_free(E);
|
- if (E != NULL) rte_swx_table_selector_free(E);
+ rte_swx_table_selector_free(E);
|
- if (E != NULL) rte_table_action_free(E);
+ rte_table_action_free(E);
|
- if (E != NULL) rte_table_action_profile_free(E);
+ rte_table_action_profile_free(E);
|
- if (E != NULL) rte_tel_data_free(E);
+ rte_tel_data_free(E);
|
- if (E != NULL) trie_free(E);
+ trie_free(E);
|
- if (E != NULL) EVP_PKEY_CTX_free(E);
+ EVP_PKEY_CTX_free(E);
|
- if (E != NULL) EVP_PKEY_free(E);
+ EVP_PKEY_free(E);
|
- if (E != NULL) BN_free(E);
+ BN_free(E);
)
