/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#include <rte_malloc.h>
#include <rte_regexdev.h>

#include "cn9k_regexdev.h"
#include "cn9k_regexdev_compiler.h"

#ifdef REE_COMPILER_SDK
#include <rxp-compiler.h>

static int
ree_rule_db_compile(const struct rte_regexdev_rule *rules,
		uint16_t nb_rules, struct rxp_rof **rof, struct rxp_rof **rofi,
		struct rxp_rof *rof_for_incremental_compile,
		struct rxp_rof *rofi_for_incremental_compile)
{
	/*INPUT*/
	struct rxp_prefix_selection_control_list *prefix_selection_control_list
		= NULL;
	struct rxp_blacklist_data_sample *blacklist_sample_data = NULL;
	struct rxp_rule_ids_to_remove *rule_ids_to_remove = NULL;
	struct rxp_roff *roff_for_incremental_compile = NULL;

	/*OPTIONS - setting default values*/
	enum rxp_virtual_prefix_mode virtual_prefix_mode =
			RXP_VIRTUAL_PREFIX_MODE_0;
	enum rxp_prefix_capacity prefix_capacity = RXP_PREFIX_CAPACITY_32K;
	/**< rxp_global_regex_options_flags*/
	enum rxp_compiler_objective objective = RXP_COMPILER_OBJECTIVE_5;
	enum rxp_tpe_data_width tpe_data_width = RXP_TPE_DATA_WIDTH_4;
	uint32_t compiler_options = RXP_COMPILER_OPTIONS_FORCE;
	/**< rxp_compiler_options_flags*/
	enum rxp_verbose_level verbose = RXP_VERBOSE_LEVEL_3;
	enum rxp_version set_rxp_version = RXP_VERSION_V5_8;
	uint32_t compiler_output_flags = 0;
	/**< rxp_compiler_output_flags*/
	uint32_t global_regex_options = 0;
	/**< rxp_global_regex_options_flags*/
	float set_auto_blacklist = 0;
	uint32_t max_rep_max = 65535;
	uint32_t divide_ruleset = 1;
	struct rxp_ruleset ruleset;
	float ptpb_threshold = 0;
	uint32_t set_max = 0;
	uint32_t threads = 1;

	/*OUTPUT*/
	struct rxp_rule_direction_analysis *rule_direction_analysis = NULL;
	struct rxp_compilation_statistics *compilation_statistics = NULL;
	struct rxp_prefix_selection_control_list *generated_pscl = NULL;
	struct rxp_uncompiled_rules_log *uncompiled_rules_log = NULL;
	struct rxp_critical_rules_rank *critical_rules_rank = NULL;
	struct rxp_compiled_rules_log *compiled_rules_log = NULL;
	struct rxp_roff *roff = NULL;

	uint16_t i;
	int ret;

	ruleset.number_of_entries = nb_rules;
	ruleset.rules = rte_malloc("rxp_rule_entry",
			nb_rules*sizeof(struct rxp_rule_entry), 0);

	if (ruleset.rules == NULL) {
		cn9k_err("Could not allocate memory for rule compilation\n");
		return -EFAULT;
	}
	if (rof_for_incremental_compile)
		compiler_options |= RXP_COMPILER_OPTIONS_INCREMENTAL;
	if (rofi_for_incremental_compile)
		compiler_options |= RXP_COMPILER_OPTIONS_CHECKSUM;

	for (i = 0; i < nb_rules; i++) {
		ruleset.rules[i].number_of_prefix_entries = 0;
		ruleset.rules[i].prefix = NULL;
		ruleset.rules[i].rule = rules[i].pcre_rule;
		ruleset.rules[i].rule_id = rules[i].rule_id;
		ruleset.rules[i].subset_id = rules[i].group_id;
		ruleset.rules[i].rule_direction_type =
				RXP_RULE_DIRECTION_TYPE_NONE;
	}

	ret = rxp_compile_advanced(
			/*INPUT*/
			&ruleset,
			prefix_selection_control_list,
			rof_for_incremental_compile,
			roff_for_incremental_compile,
			rofi_for_incremental_compile,
			rule_ids_to_remove,
			blacklist_sample_data,

			/*OPTIONS*/
			compiler_options,
			prefix_capacity,
			global_regex_options,
			set_auto_blacklist,
			set_max,
			objective,
			ptpb_threshold,
			max_rep_max,
			threads,
			set_rxp_version,
			verbose,
			tpe_data_width,
			virtual_prefix_mode,
			compiler_output_flags,
			divide_ruleset,

			/*OUTPUT*/
			&compilation_statistics,
			&compiled_rules_log,
			&critical_rules_rank,
			&rule_direction_analysis,
			&uncompiled_rules_log,
			rof,
			&roff,
			rofi,
			&generated_pscl);
	rte_free(ruleset.rules);

	return ret;
}

int
cn9k_ree_rule_db_compile_prog(struct rte_regexdev *dev)
{
	struct cn9k_ree_data *data = dev->data->dev_private;
	struct roc_ree_vf *vf = &data->vf;
	char compiler_version[] = "20.5.2.eda0fa2";
	char timestamp[] = "19700101_000001";
	uint32_t rule_db_len, rule_dbi_len;
	struct rxp_rof *rofi_inc_p = NULL;
	struct rxp_rof_entry rule_dbi[6];
	char *rofi_rof_entries = NULL;
	struct rxp_rof *rofi = NULL;
	struct rxp_rof *rof = NULL;
	struct rxp_rof rofi_inc;
	struct rxp_rof rof_inc;
	char *rule_db = NULL;
	int ret;

	ree_func_trace();

	ret = roc_ree_rule_db_len_get(vf, &rule_db_len, &rule_dbi_len);
	if (ret != 0) {
		cn9k_err("Could not get rule db length");
		return ret;
	}

	if (rule_db_len > 0) {
		cn9k_ree_dbg("Incremental compile, rule db len %d rule dbi len %d",
				rule_db_len, rule_dbi_len);
		rule_db = rte_malloc("ree_rule_db", rule_db_len, 0);
		if (!rule_db) {
			cn9k_err("Could not allocate memory for rule db");
			return -EFAULT;
		}

		ret = roc_ree_rule_db_get(vf, rule_db, rule_db_len,
				(char *)rule_dbi, rule_dbi_len);
		if (ret) {
			cn9k_err("Could not read rule db");
			rte_free(rule_db);
			return -EFAULT;
		}
		rof_inc.rof_revision = 0;
		rof_inc.rof_version = 2;
		rof_inc.rof_entries = (struct rxp_rof_entry *)rule_db;
		rof_inc.rxp_compiler_version = compiler_version;
		rof_inc.timestamp = timestamp;
		rof_inc.number_of_entries =
				(rule_db_len/sizeof(struct rxp_rof_entry));

		if (rule_dbi_len > 0) {
			/* incremental compilation not the first time */
			rofi_inc.rof_revision = 0;
			rofi_inc.rof_version = 2;
			rofi_inc.rof_entries = rule_dbi;
			rofi_inc.rxp_compiler_version = compiler_version;
			rofi_inc.timestamp = timestamp;
			rofi_inc.number_of_entries =
				(rule_dbi_len/sizeof(struct rxp_rof_entry));
			rofi_inc_p = &rofi_inc;
		}
		ret = ree_rule_db_compile(data->rules, data->nb_rules, &rof,
				&rofi, &rof_inc, rofi_inc_p);
		if (rofi->number_of_entries == 0) {
			cn9k_ree_dbg("No change to rule db");
			ret = 0;
			goto free_structs;
		}
		rule_dbi_len = rofi->number_of_entries *
				sizeof(struct rxp_rof_entry);
		rofi_rof_entries = (char *)rofi->rof_entries;
	} else {
		/* full compilation */
		ret = ree_rule_db_compile(data->rules, data->nb_rules, &rof,
				&rofi, NULL, NULL);
	}
	if (ret != 0) {
		cn9k_err("Could not compile rule db");
		goto free_structs;
	}
	rule_db_len = rof->number_of_entries * sizeof(struct rxp_rof_entry);
	ret = roc_ree_rule_db_prog(vf, (char *)rof->rof_entries, rule_db_len,
			rofi_rof_entries, rule_dbi_len);
	if (ret)
		cn9k_err("Could not program rule db");

free_structs:
	rxp_free_structs(NULL, NULL, NULL, NULL, NULL, &rof, NULL, &rofi, NULL,
			1);

	rte_free(rule_db);

	return ret;
}
#else
int
cn9k_ree_rule_db_compile_prog(struct rte_regexdev *dev)
{
	RTE_SET_USED(dev);
	return -ENOTSUP;
}
#endif
