/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022, Marvell
 */

#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_cryptodev.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_security.h>

#include <app/test/test_cryptodev.h>
#include <app/test/test_cryptodev_security_ipsec.h>
#include <app/test/test_cryptodev_security_ipsec_test_vectors.h>

#define NB_DESC 4096
#define DEF_NB_SESSIONS (16 * 10 * 1024) /* 16 * 10K tunnels */

struct lcore_conf {
	struct rte_crypto_sym_xform cipher_xform;
	struct rte_crypto_sym_xform auth_xform;
	struct rte_crypto_sym_xform aead_xform;
	uint8_t dev_id;
	uint8_t qp_id;
	struct test_ctx *ctx;
};

struct test_ctx {
	struct lcore_conf lconf[RTE_MAX_LCORE];
	void *sec_ctx;
	struct rte_mempool *sess_mp;
	struct ipsec_test_data *td;
	int nb_sess;
	unsigned long td_idx;
	uint8_t nb_lcores;
	uint8_t nb_cryptodevs;
	uint8_t enabled_cdevs[RTE_CRYPTO_MAX_DEVS];
	bool is_inbound;
};

static struct test_ctx ctx;

static int
cryptodev_init(struct test_ctx *ctx, uint8_t nb_lcores)
{
	const char dev_names[][RTE_CRYPTODEV_NAME_MAX_LEN] = {
		"crypto_cn10k",
		"crypto_cn9k",
		"crypto_dpaa_sec",
		"crypto_dpaa2_sec",
	};
	struct rte_cryptodev_qp_conf qp_conf;
	struct rte_cryptodev_info dev_info;
	struct rte_cryptodev_config config;
	unsigned int j, nb_qp, qps_reqd;
	uint8_t socket_id;
	uint32_t dev_cnt;
	int ret, core_id;
	void *sec_ctx;
	uint64_t i;

	i = 0;
	do {
		dev_cnt = rte_cryptodev_devices_get(dev_names[i],
						     ctx->enabled_cdevs,
						     RTE_CRYPTO_MAX_DEVS);
		i++;
	} while (dev_cnt == 0 && i < RTE_DIM(dev_names));

	if (dev_cnt == 0)
		return -1;

	/* Check first device for capabilities */
	rte_cryptodev_info_get(0, &dev_info);
	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SECURITY)) {
		RTE_LOG(ERR, USER1,
			"Security not supported by the cryptodev\n");
		return -1;
	}

	sec_ctx = rte_cryptodev_get_sec_ctx(0);
	ctx->sec_ctx = sec_ctx;

	socket_id = rte_socket_id();
	qps_reqd = nb_lcores;
	core_id = 0;
	i = 0;

	do {
		rte_cryptodev_info_get(i, &dev_info);
		qps_reqd = RTE_MIN(dev_info.max_nb_queue_pairs, qps_reqd);

		for (j = 0; j < qps_reqd; j++) {
			ctx->lconf[core_id].dev_id = i;
			ctx->lconf[core_id].qp_id = j;
			ctx->lconf[core_id].ctx = ctx;
			core_id++;
			if (core_id == RTE_MAX_LCORE)
				break;
		}

		nb_qp = j;

		memset(&config, 0, sizeof(config));
		config.nb_queue_pairs = nb_qp;
		config.socket_id = socket_id;

		ret = rte_cryptodev_configure(i, &config);
		if (ret < 0) {
			RTE_LOG(ERR, USER1,
				"Could not configure cryptodev - %" PRIu64 "\n",
				i);
			return -1;
		}

		memset(&qp_conf, 0, sizeof(qp_conf));
		qp_conf.nb_descriptors = NB_DESC;

		for (j = 0; j < nb_qp; j++) {
			ret = rte_cryptodev_queue_pair_setup(i, j, &qp_conf,
							     socket_id);
			if (ret < 0) {
				RTE_LOG(ERR, USER1,
					"Could not configure queue pair:"
					" %" PRIu64 " - %d\n", i, j);
				return -1;
			}
		}

		ret = rte_cryptodev_start(i);
		if (ret < 0) {
			RTE_LOG(ERR, USER1, "Could not start cryptodev\n");
			return -1;
		}

		i++;
		qps_reqd -= j;

	} while (i < dev_cnt && core_id < RTE_MAX_LCORE);

	ctx->nb_cryptodevs = i;

	return 0;
}

static int
cryptodev_fini(struct test_ctx *ctx)
{
	int i, ret = 0;

	for (i = 0; i < ctx->nb_cryptodevs &&
			i < RTE_CRYPTO_MAX_DEVS; i++) {
		rte_cryptodev_stop(ctx->enabled_cdevs[i]);
		ret = rte_cryptodev_close(ctx->enabled_cdevs[i]);
		if (ret)
			RTE_LOG(ERR, USER1,
					"Crypto device close error %d\n", ret);
	}

	return ret;
}

static int
mempool_init(struct test_ctx *ctx, uint8_t nb_lcores)
{
	struct rte_mempool *sess_mpool;
	unsigned int sec_sess_sz;
	int nb_sess_total;

	nb_sess_total = ctx->nb_sess + RTE_MEMPOOL_CACHE_MAX_SIZE * nb_lcores;

	sec_sess_sz = rte_security_session_get_size(ctx->sec_ctx);

	sess_mpool = rte_cryptodev_sym_session_pool_create("test_sess_mp",
			nb_sess_total, sec_sess_sz, RTE_MEMPOOL_CACHE_MAX_SIZE,
			0, SOCKET_ID_ANY);
	if (sess_mpool == NULL) {
		RTE_LOG(ERR, USER1, "Could not create mempool\n");
		return -1;
	}

	ctx->sess_mp = sess_mpool;

	return 0;
}

static int
mempool_fini(struct test_ctx *ctx)
{
	rte_mempool_free(ctx->sess_mp);

	return 0;
}

static int
sec_conf_init(struct lcore_conf *conf,
	      struct rte_security_session_conf *sess_conf,
	      struct rte_security_ipsec_xform *ipsec_xform,
	      const struct ipsec_test_data *td)
{
	uint16_t v6_src[8] = {0x2607, 0xf8b0, 0x400c, 0x0c03, 0x0000, 0x0000,
				0x0000, 0x001a};
	uint16_t v6_dst[8] = {0x2001, 0x0470, 0xe5bf, 0xdead, 0x4957, 0x2174,
				0xe82c, 0x4887};
	const struct rte_ipv4_hdr *ipv4 =
			(const struct rte_ipv4_hdr *)td->output_text.data;
	struct rte_security_capability_idx sec_cap_idx;
	const struct rte_security_capability *sec_cap;
	enum rte_security_ipsec_sa_direction dir;
	uint32_t src, dst;
	int salt_len;

	/* Copy IPsec xform */
	memcpy(ipsec_xform, &td->ipsec_xform, sizeof(*ipsec_xform));

	dir = ipsec_xform->direction;

	memcpy(&src, &ipv4->src_addr, sizeof(ipv4->src_addr));
	memcpy(&dst, &ipv4->dst_addr, sizeof(ipv4->dst_addr));

	if (td->ipsec_xform.mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL) {
		if (td->ipsec_xform.tunnel.type ==
				RTE_SECURITY_IPSEC_TUNNEL_IPV4) {
			memcpy(&ipsec_xform->tunnel.ipv4.src_ip, &src,
			       sizeof(src));
			memcpy(&ipsec_xform->tunnel.ipv4.dst_ip, &dst,
			       sizeof(dst));

		} else {
			memcpy(&ipsec_xform->tunnel.ipv6.src_addr, &v6_src,
			       sizeof(v6_src));
			memcpy(&ipsec_xform->tunnel.ipv6.dst_addr, &v6_dst,
			       sizeof(v6_dst));
		}
	}

	sec_cap_idx.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL;
	sec_cap_idx.protocol = RTE_SECURITY_PROTOCOL_IPSEC;
	sec_cap_idx.ipsec.proto = ipsec_xform->proto;
	sec_cap_idx.ipsec.mode = ipsec_xform->mode;
	sec_cap_idx.ipsec.direction = ipsec_xform->direction;

	sec_cap = rte_security_capability_get(conf->ctx->sec_ctx, &sec_cap_idx);
	if (sec_cap == NULL) {
		RTE_LOG(ERR, USER1, "Could not get capabilities\n");
		return -1;
	}

	/* Copy cipher session parameters */
	if (td[0].aead) {
		memcpy(&conf->aead_xform, &td[0].xform.aead,
		       sizeof(conf->aead_xform));
		conf->aead_xform.aead.key.data = td[0].key.data;
		conf->aead_xform.aead.iv.offset = IV_OFFSET;

		/* Verify crypto capabilities */
		if (test_ipsec_crypto_caps_aead_verify(
				sec_cap,
				&conf->aead_xform) != 0) {
			RTE_LOG(ERR, USER1,
				"Crypto capabilities not supported\n");
			return -1;
		}
	} else if (td[0].auth_only) {
		memcpy(&conf->auth_xform, &td[0].xform.chain.auth,
		       sizeof(conf->auth_xform));
		conf->auth_xform.auth.key.data = td[0].auth_key.data;

		if (test_ipsec_crypto_caps_auth_verify(
				sec_cap,
				&conf->auth_xform) != 0) {
			RTE_LOG(INFO, USER1,
				"Auth crypto capabilities not supported\n");
			return -1;
		}
	} else {
		memcpy(&conf->cipher_xform, &td[0].xform.chain.cipher,
		       sizeof(conf->cipher_xform));
		memcpy(&conf->auth_xform, &td[0].xform.chain.auth,
		       sizeof(conf->auth_xform));
		conf->cipher_xform.cipher.key.data = td[0].key.data;
		conf->cipher_xform.cipher.iv.offset = IV_OFFSET;
		conf->auth_xform.auth.key.data = td[0].auth_key.data;

		/* Verify crypto capabilities */

		if (test_ipsec_crypto_caps_cipher_verify(
				sec_cap,
				&conf->cipher_xform) != 0) {
			RTE_LOG(ERR, USER1,
				"Cipher crypto capabilities not supported\n");
			return -1;
		}

		if (test_ipsec_crypto_caps_auth_verify(
				sec_cap,
				&conf->auth_xform) != 0) {
			RTE_LOG(ERR, USER1,
				"Auth crypto capabilities not supported\n");
			return -1;
		}
	}

	if (test_ipsec_sec_caps_verify(ipsec_xform, sec_cap, 0) != 0)
		return -1;

	sess_conf->action_type = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL;
	sess_conf->protocol = RTE_SECURITY_PROTOCOL_IPSEC;

	if (td[0].aead || td[0].aes_gmac) {
		salt_len = RTE_MIN(sizeof(ipsec_xform->salt), td[0].salt.len);
		memcpy(&ipsec_xform->salt, td[0].salt.data, salt_len);
	}

	if (td[0].aead) {
		sess_conf->ipsec = *ipsec_xform;
		sess_conf->crypto_xform = &conf->aead_xform;
	} else if (td[0].auth_only) {
		sess_conf->ipsec = *ipsec_xform;
		sess_conf->crypto_xform = &conf->auth_xform;
	} else {
		sess_conf->ipsec = *ipsec_xform;
		if (dir == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
			sess_conf->crypto_xform = &conf->cipher_xform;
			conf->cipher_xform.next = &conf->auth_xform;
		} else {
			sess_conf->crypto_xform = &conf->auth_xform;
			conf->auth_xform.next = &conf->cipher_xform;
		}
	}

	return 0;
}

static int
test_security_session_perf(void *arg)
{
	uint64_t tsc_start, tsc_mid, tsc_end, tsc_setup_dur, tsc_destroy_dur;
	struct rte_security_ipsec_xform ipsec_xform;
	struct rte_security_session_conf sess_conf;
	int i, ret, nb_sessions, nb_sess_total;
	struct rte_security_session **sess;
	void *sec_ctx;
	double setup_rate, destroy_rate;
	uint64_t setup_ms, destroy_ms;
	struct lcore_conf *conf = arg;
	struct rte_mempool *sess_mp;
	uint8_t nb_lcores;

	nb_lcores = conf->ctx->nb_lcores;
	nb_sess_total = conf->ctx->nb_sess;
	sec_ctx = conf->ctx->sec_ctx;
	sess_mp = conf->ctx->sess_mp;

	nb_sessions = nb_sess_total / nb_lcores;

	if (conf->qp_id == 0)
		nb_sessions += (nb_sess_total - nb_sessions * nb_lcores);

	ret = sec_conf_init(conf, &sess_conf, &ipsec_xform,
			    &ctx.td[ctx.td_idx]);
	if (ret) {
		RTE_LOG(ERR, USER1, "Could not initialize session conf\n");
		return EXIT_FAILURE;
	}

	sess = rte_zmalloc(NULL, sizeof(void *) * nb_sessions, 0);

	tsc_start = rte_rdtsc_precise();

	for (i = 0; i < nb_sessions; i++) {
		sess[i] = rte_security_session_create(sec_ctx,
						      &sess_conf,
						      sess_mp);
		if (unlikely(sess[i] == NULL)) {
			RTE_LOG(ERR, USER1, "Could not create session\n");
			return EXIT_FAILURE;
		}
	}

	tsc_mid = rte_rdtsc_precise();

	for (i = 0; i < nb_sessions; i++) {
		ret = rte_security_session_destroy(sec_ctx, sess[i]);
		if (unlikely(ret < 0)) {
			RTE_LOG(ERR, USER1, "Could not destroy session\n");
			return EXIT_FAILURE;
		}
	}

	tsc_end = rte_rdtsc_precise();

	tsc_setup_dur = tsc_mid - tsc_start;
	tsc_destroy_dur = tsc_end - tsc_mid;

	setup_ms = tsc_setup_dur * 1000 / rte_get_tsc_hz();
	destroy_ms = tsc_destroy_dur * 1000 / rte_get_tsc_hz();

	setup_rate = (double)nb_sessions * rte_get_tsc_hz() / tsc_setup_dur;
	destroy_rate = (double)nb_sessions * rte_get_tsc_hz() / tsc_destroy_dur;

	printf("%20u%20u%20"PRIu64"%20"PRIu64"%20.2f%20.2f\n",
			rte_lcore_id(),
			nb_sessions,
			setup_ms,
			destroy_ms,
			setup_rate,
			destroy_rate);

	return EXIT_SUCCESS;
}

static void
usage(char *progname)
{
	printf("\nusage: %s\n", progname);
	printf("  --help     : display this message and exit\n"
	       "  --inbound  : test for inbound direction\n"
		"           default outbound direction is tested\n"
	       "  --nb-sess=N: to set the number of sessions\n"
		"           to be created, default is %d\n", DEF_NB_SESSIONS);
}

static void
args_parse(int argc, char **argv)
{
	char **argvopt;
	int n, opt;
	int opt_idx;

	static const struct option lgopts[] = {
		/* Control */
		{ "help",    0, 0, 0 },
		{ "inbound", 0, 0, 0 },
		{ "nb-sess", 1, 0, 0 },
		{ NULL, 0, 0, 0 }
	};

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "",
				lgopts, &opt_idx)) != EOF) {
		switch (opt) {
		case 0:
			if (strcmp(lgopts[opt_idx].name, "help") == 0) {
				usage(argv[0]);
				exit(EXIT_SUCCESS);
			}

			if (strcmp(lgopts[opt_idx].name, "nb-sess") == 0) {
				n = atoi(optarg);
				if (n >= 0)
					ctx.nb_sess = n;
				else
					rte_exit(EXIT_FAILURE,
						"nb-sess should be >= 0\n");
				printf("nb-sess %d / ", ctx.nb_sess);
			} else if (strcmp(lgopts[opt_idx].name, "inbound") ==
				   0) {
				ctx.is_inbound = true;
				printf("inbound / ");
			}

			break;

		default:
			usage(argv[0]);
			rte_exit(EXIT_FAILURE, "Invalid option: %s\n",
					argv[opt_idx - 1]);
			break;
		}
	}

	printf("\n\n");
}

int
main(int argc, char **argv)
{
	struct ipsec_test_data td_outb[RTE_DIM(alg_list)];
	struct ipsec_test_data td_inb[RTE_DIM(alg_list)];
	struct ipsec_test_flags flags;
	uint32_t lcore_id;
	uint8_t nb_lcores;
	unsigned long i;
	int ret;

	memset(&ctx, 0, sizeof(struct test_ctx));
	memset(&flags, 0, sizeof(flags));

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments!\n");
	argc -= ret;
	argv += ret;

	nb_lcores = rte_lcore_count() - 1;
	if (nb_lcores < 1) {
		RTE_LOG(ERR, USER1,
			"Number of worker cores need to be higher than 1\n");
		return -EINVAL;
	}

	ctx.nb_sess = DEF_NB_SESSIONS + RTE_MEMPOOL_CACHE_MAX_SIZE * nb_lcores;

	if (argc > 1)
		args_parse(argc, argv);

	ctx.nb_lcores = nb_lcores;

	ret = cryptodev_init(&ctx, nb_lcores);
	if (ret)
		goto exit;

	ret = mempool_init(&ctx, nb_lcores);
	if (ret)
		goto cryptodev_fini;

	test_ipsec_alg_list_populate();

	for (i = 0; i < RTE_DIM(alg_list); i++) {
		test_ipsec_td_prepare(alg_list[i].param1,
				      alg_list[i].param2,
				      &flags,
				      &td_outb[i],
				      1);
		if (ctx.is_inbound)
			test_ipsec_td_in_from_out(&td_outb[i], &td_inb[i]);
	}

	ctx.td = td_outb;
	if (ctx.is_inbound)
		ctx.td = td_inb;

	for (ctx.td_idx = 0; ctx.td_idx < RTE_DIM(alg_list); ctx.td_idx++) {

		printf("\n\n    Algorithm combination:");
		test_ipsec_display_alg(alg_list[ctx.td_idx].param1,
				       alg_list[ctx.td_idx].param2);
		printf("    ----------------------");

		printf("\n%20s%20s%20s%20s%20s%20s\n\n",
			"lcore id", "nb_sessions",
			"Setup time(ms)", "Destroy time(ms)",
			"Setup rate(sess/s)",
			"Destroy rate(sess/sec)");

		i = 0;
		RTE_LCORE_FOREACH_WORKER(lcore_id) {
			rte_eal_remote_launch(test_security_session_perf,
					      &ctx.lconf[i],
					      lcore_id);
			i++;
		}

		RTE_LCORE_FOREACH_WORKER(lcore_id) {
			ret |= rte_eal_wait_lcore(lcore_id);
		}

	}

	cryptodev_fini(&ctx);
	mempool_fini(&ctx);

	return EXIT_SUCCESS;
cryptodev_fini:
	cryptodev_fini(&ctx);
exit:
	return EXIT_FAILURE;

}
