@rule1@
identifier I =~  "^(RTE_FC_|ETH_MQ_|ETH_RSS|DEV_RX_|DEV_TX_|ETH_LINK|RTE_RETA|
|ETH_DCB|RTE_TUNNEL|ETH_VLAN|ETH_4|ETH_8|ETH_16|ETH_32|ETH_64|RTE_FDIR|RTE_L2|
|ETH_SPEED_NUM|ETH_TUNNEL_FILT|ETH_RSS_RETA_|ETH_VMDQ|ETH_NUM|ETH_QINQ|
|ETH_MAX_)";
@@
I

@ script : python p@
I << rule1.I;
J;
@@
coccinelle .J="RTE_ETH_" + I[4:];

exception_matches = ["ETH_VLAN_FILTER_CLASSIFY","ETH_VLAN_FILTER_ANY",
"ETH_VLAN_FILTER_SPEC","ETH_RSS_MODE","ETH_RSS_UPDATE","RTE_FDIR_MODE",
"RTE_FDIR_NO","RTE_FDIR_REPORT","ETH_MAX_RX_CLIENTS_E1H",
"ETH_MAX_AGGREGATION_QUEUES_E1","ETH_RSS_ENGINE_NUM","ETH_NUM_MAC_FILTERS",
"ETH_MAX_NUM_RX_QUEUES_PER_VF_QUAD","ETH_RSS_IND_TABLE_ENTRIES_NUM",
"ETH_RSS_KEY_SIZE_REGS","ETH_NUM_STATISTIC_COUNTERS"]

if any(x in I for x in exception_matches):
        coccinelle .J= I;

@ identifier@
identifier rule1.I;
identifier p.J;
@@
- I
+ J

// Below rule for structures only
@rule2@
identifier A  =~  "rte_fdir_conf|rte_intr_conf";
@@
struct A

@ script : python p2@
A << rule2.A;
B;
@@
coccinelle .B="rte_eth_" + A[4:];

@ identifier2@
identifier rule2.A;
identifier p2.B;
@@
- struct A
+ struct B
