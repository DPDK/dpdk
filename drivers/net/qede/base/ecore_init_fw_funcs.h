/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef _INIT_FW_FUNCS_H
#define _INIT_FW_FUNCS_H
/* forward declarations */
struct init_qm_pq_params;
/**
 * @brief ecore_qm_pf_mem_size - prepare QM ILT sizes
 *
 * Returns the required host memory size in 4KB units.
 * Must be called before all QM init HSI functions.
 *
 * @param pf_id			- physical function ID
 * @param num_pf_cids	- number of connections used by this PF
 * @param num_vf_cids	- number of connections used by VFs of this PF
 * @param num_tids		- number of tasks used by this PF
 * @param num_pf_pqs	- number of PQs used by this PF
 * @param num_vf_pqs	- number of PQs used by VFs of this PF
 *
 * @return The required host memory size in 4KB units.
 */
u32 ecore_qm_pf_mem_size(u8 pf_id,
			 u32 num_pf_cids,
			 u32 num_vf_cids,
			 u32 num_tids, u16 num_pf_pqs, u16 num_vf_pqs);
/**
 * @brief ecore_qm_common_rt_init -
 * Prepare QM runtime init values for the engine phase
 *
 * @param p_hwfn
 * @param max_ports_per_engine	- max number of ports per engine in HW
 * @param max_phys_tcs_per_port	- max number of physical TCs per port in HW
 * @param pf_rl_en				- enable per-PF rate limiters
 * @param pf_wfq_en				- enable per-PF WFQ
 * @param vport_rl_en			- enable per-VPORT rate limiters
 * @param vport_wfq_en			- enable per-VPORT WFQ
 * @param port_params- array of size MAX_NUM_PORTS with parameters for each port
 *
 * @return 0 on success, -1 on error.
 */
int ecore_qm_common_rt_init(struct ecore_hwfn *p_hwfn,
			    u8 max_ports_per_engine,
			    u8 max_phys_tcs_per_port,
			    bool pf_rl_en,
			    bool pf_wfq_en,
			    bool vport_rl_en,
			    bool vport_wfq_en,
			    struct init_qm_port_params
			    port_params[MAX_NUM_PORTS]);

int ecore_qm_pf_rt_init(struct ecore_hwfn *p_hwfn,
			struct ecore_ptt *p_ptt,
			u8 port_id,
			u8 pf_id,
			u8 max_phys_tcs_per_port,
			bool is_first_pf,
			u32 num_pf_cids,
			u32 num_vf_cids,
			u32 num_tids,
			u16 start_pq,
			u16 num_pf_pqs,
			u16 num_vf_pqs,
			u8 start_vport,
			u8 num_vports,
			u16 pf_wfq,
			u32 pf_rl,
			struct init_qm_pq_params *pq_params,
			struct init_qm_vport_params *vport_params);
/**
 * @brief ecore_init_pf_wfq  Initializes the WFQ weight of the specified PF
 *
 * @param p_hwfn
 * @param p_ptt		- ptt window used for writing the registers
 * @param pf_id		- PF ID
 * @param pf_wfq	- WFQ weight. Must be non-zero.
 *
 * @return 0 on success, -1 on error.
 */
int ecore_init_pf_wfq(struct ecore_hwfn *p_hwfn,
		      struct ecore_ptt *p_ptt, u8 pf_id, u16 pf_wfq);
/**
 * @brief ecore_init_pf_rl  Initializes the rate limit of the specified PF
 *
 * @param p_hwfn
 * @param p_ptt	- ptt window used for writing the registers
 * @param pf_id	- PF ID
 * @param pf_rl	- rate limit in Mb/sec units
 *
 * @return 0 on success, -1 on error.
 */
int ecore_init_pf_rl(struct ecore_hwfn *p_hwfn,
		     struct ecore_ptt *p_ptt, u8 pf_id, u32 pf_rl);
/**
 * @brief ecore_init_vport_wfq Initializes the WFQ weight of the specified VPORT
 *
 * @param p_hwfn
 * @param p_ptt			- ptt window used for writing the registers
 * @param first_tx_pq_id- An array containing the first Tx PQ ID associated
 *                        with the VPORT for each TC. This array is filled by
 *                        ecore_qm_pf_rt_init
 * @param vport_wfq		- WFQ weight. Must be non-zero.
 *
 * @return 0 on success, -1 on error.
 */
int ecore_init_vport_wfq(struct ecore_hwfn *p_hwfn,
			 struct ecore_ptt *p_ptt,
			 u16 first_tx_pq_id[NUM_OF_TCS], u16 vport_wfq);
/**
 * @brief ecore_init_vport_rl  Initializes the rate limit of the specified VPORT
 *
 * @param p_hwfn
 * @param p_ptt		- ptt window used for writing the registers
 * @param vport_id	- VPORT ID
 * @param vport_rl	- rate limit in Mb/sec units
 *
 * @return 0 on success, -1 on error.
 */
int ecore_init_vport_rl(struct ecore_hwfn *p_hwfn,
			struct ecore_ptt *p_ptt, u8 vport_id, u32 vport_rl);
/**
 * @brief ecore_send_qm_stop_cmd  Sends a stop command to the QM
 *
 * @param p_hwfn
 * @param p_ptt	         - ptt window used for writing the registers
 * @param is_release_cmd - true for release, false for stop.
 * @param is_tx_pq       - true for Tx PQs, false for Other PQs.
 * @param start_pq       - first PQ ID to stop
 * @param num_pqs        - Number of PQs to stop, starting from start_pq.
 *
 * @return bool, true if successful, false if timeout occurred while
 * waiting for QM command done.
 */
bool ecore_send_qm_stop_cmd(struct ecore_hwfn *p_hwfn,
			    struct ecore_ptt *p_ptt,
			    bool is_release_cmd,
			    bool is_tx_pq, u16 start_pq, u16 num_pqs);
/**
 * @brief ecore_init_nig_ets - initializes the NIG ETS arbiter
 *
 * Based on weight/priority requirements per-TC.
 *
 * @param p_ptt	- ptt window used for writing the registers.
 * @param req	- the NIG ETS initialization requirements.
 * @param is_lb	- if set, the loopback port arbiter is initialized, otherwise
 *		  the physical port arbiter is initialized. The pure-LB TC
 *		  requirements are ignored when is_lb is cleared.
 */
void ecore_init_nig_ets(struct ecore_hwfn *p_hwfn,
			struct ecore_ptt *p_ptt,
			struct init_ets_req *req, bool is_lb);
/**
 * @brief ecore_init_nig_lb_rl - initializes the NIG LB RLs
 *
 * Based on global and per-TC rate requirements
 *
 * @param p_ptt	- ptt window used for writing the registers.
 * @param req	- the NIG LB RLs initialization requirements.
 */
void ecore_init_nig_lb_rl(struct ecore_hwfn *p_hwfn,
			  struct ecore_ptt *p_ptt,
			  struct init_nig_lb_rl_req *req);
/**
 * @brief ecore_init_nig_pri_tc_map - initializes the NIG priority to TC map.
 *
 * Assumes valid arguments.
 *
 * @param p_ptt	- ptt window used for writing the registers.
 * @param req	- required mapping from prioirties to TCs.
 */
void ecore_init_nig_pri_tc_map(struct ecore_hwfn *p_hwfn,
			       struct ecore_ptt *p_ptt,
			       struct init_nig_pri_tc_map_req *req);
/**
 * @brief ecore_init_prs_ets - initializes the PRS Rx ETS arbiter
 *
 * Based on weight/priority requirements per-TC.
 *
 * @param p_ptt	- ptt window used for writing the registers.
 * @param req	- the PRS ETS initialization requirements.
 */
void ecore_init_prs_ets(struct ecore_hwfn *p_hwfn,
			struct ecore_ptt *p_ptt, struct init_ets_req *req);
/**
 * @brief ecore_init_brb_ram - initializes BRB RAM sizes per TC
 *
 * Based on weight/priority requirements per-TC.
 *
 * @param p_ptt	- ptt window used for writing the registers.
 * @param req	- the BRB RAM initialization requirements.
 */
void ecore_init_brb_ram(struct ecore_hwfn *p_hwfn,
			struct ecore_ptt *p_ptt, struct init_brb_ram_req *req);
/**
 * @brief ecore_set_engine_mf_ovlan_eth_type - initializes Nig,Prs,Pbf
 * and llh ethType Regs to  input ethType
 * should Be called once per engine if engine is in BD mode.
 *
 * @param p_ptt    - ptt window used for writing the registers.
 * @param ethType - etherType to configure
 */
void ecore_set_engine_mf_ovlan_eth_type(struct ecore_hwfn *p_hwfn,
					struct ecore_ptt *p_ptt, u32 eth_type);
/**
 * @brief ecore_set_port_mf_ovlan_eth_type - initializes DORQ ethType Regs
 * to input ethType
 * should Be called once per port.
 *
 * @param p_ptt    - ptt window used for writing the registers.
 * @param ethType - etherType to configure
 */
void ecore_set_port_mf_ovlan_eth_type(struct ecore_hwfn *p_hwfn,
				      struct ecore_ptt *p_ptt, u32 eth_type);
/**
 * @brief ecore_set_vxlan_dest_port - init vxlan tunnel destination udp port
 *
 * @param p_ptt     - ptt window used for writing the registers.
 * @param dest_port - vxlan destination udp port.
 */
void ecore_set_vxlan_dest_port(struct ecore_hwfn *p_hwfn,
			       struct ecore_ptt *p_ptt, u16 dest_port);
/**
 * @brief ecore_set_vxlan_enable - enable or disable VXLAN tunnel in HW
 *
 * @param p_ptt        - ptt window used for writing the registers.
 * @param vxlan_enable - vxlan enable flag.
 */
void ecore_set_vxlan_enable(struct ecore_hwfn *p_hwfn,
			    struct ecore_ptt *p_ptt, bool vxlan_enable);
/**
 * @brief ecore_set_gre_enable - enable or disable GRE tunnel in HW
 *
 * @param p_ptt          - ptt window used for writing the registers.
 * @param eth_gre_enable - eth GRE enable enable flag.
 * @param ip_gre_enable  - IP GRE enable enable flag.
 */
void ecore_set_gre_enable(struct ecore_hwfn *p_hwfn,
			  struct ecore_ptt *p_ptt,
			  bool eth_gre_enable, bool ip_gre_enable);
/**
 * @brief ecore_set_geneve_dest_port - init geneve tunnel destination udp port
 *
 * @param p_ptt     - ptt window used for writing the registers.
 * @param dest_port - geneve destination udp port.
 */
void ecore_set_geneve_dest_port(struct ecore_hwfn *p_hwfn,
				struct ecore_ptt *p_ptt, u16 dest_port);
/**
 * @brief ecore_set_gre_enable - enable or disable GRE tunnel in HW
 *
 * @param p_ptt             - ptt window used for writing the registers.
 * @param eth_geneve_enable - eth GENEVE enable enable flag.
 * @param ip_geneve_enable  - IP GENEVE enable enable flag.
  */
void ecore_set_geneve_enable(struct ecore_hwfn *p_hwfn,
			     struct ecore_ptt *p_ptt,
			     bool eth_geneve_enable, bool ip_geneve_enable);
#endif
