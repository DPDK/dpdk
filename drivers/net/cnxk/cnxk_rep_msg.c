/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <cnxk_rep.h>
#include <cnxk_rep_msg.h>

#define CTRL_MSG_RCV_TIMEOUT_MS 2000
#define CTRL_MSG_READY_WAIT_US	2000
#define CTRL_MSG_THRD_NAME_LEN	35
#define CTRL_MSG_BUFFER_SZ	1500
#define CTRL_MSG_SIGNATURE	0xcdacdeadbeefcadc

static void
close_socket(int fd)
{
	close(fd);
	unlink(CNXK_ESWITCH_CTRL_MSG_SOCK_PATH);
}

static int
receive_control_message(int socketfd, void *data, uint32_t len)
{
	char ctl[CMSG_SPACE(sizeof(int)) + CMSG_SPACE(sizeof(struct ucred))] = {0};
	struct ucred *cr __rte_unused;
	struct msghdr mh = {0};
	struct cmsghdr *cmsg;
	static uint64_t rec;
	struct iovec iov[1];
	ssize_t size;
	int afd = -1;

	iov[0].iov_base = data;
	iov[0].iov_len = len;
	mh.msg_iov = iov;
	mh.msg_iovlen = 1;
	mh.msg_control = ctl;
	mh.msg_controllen = sizeof(ctl);

	size = recvmsg(socketfd, &mh, MSG_DONTWAIT);
	if (size < 0) {
		if (errno == EAGAIN)
			return 0;
		plt_err("recvmsg err %d size %zu", errno, size);
		return -errno;
	} else if (size == 0) {
		return 0;
	}

	rec++;
	plt_rep_dbg("Packet %" PRId64 " Received %" PRId64 " bytes over socketfd %d",
		    rec, size, socketfd);

	cr = 0;
	cmsg = CMSG_FIRSTHDR(&mh);
	while (cmsg) {
		if (cmsg->cmsg_level == SOL_SOCKET) {
			if (cmsg->cmsg_type == SCM_CREDENTIALS) {
				cr = (struct ucred *)CMSG_DATA(cmsg);
			} else if (cmsg->cmsg_type == SCM_RIGHTS) {
				rte_memcpy(&afd, CMSG_DATA(cmsg), sizeof(int));
				plt_rep_dbg("afd %d", afd);
			}
		}
		cmsg = CMSG_NXTHDR(&mh, cmsg);
	}
	return size;
}

static int
send_message_on_socket(int socketfd, void *data, uint32_t len, int afd)
{
	char ctl[CMSG_SPACE(sizeof(int))];
	struct msghdr mh = {0};
	struct cmsghdr *cmsg;
	static uint64_t sent;
	struct iovec iov[1];
	int size;

	iov[0].iov_base = data;
	iov[0].iov_len = len;
	mh.msg_iov = iov;
	mh.msg_iovlen = 1;

	if (afd > 0) {
		memset(&ctl, 0, sizeof(ctl));
		mh.msg_control = ctl;
		mh.msg_controllen = sizeof(ctl);
		cmsg = CMSG_FIRSTHDR(&mh);
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		rte_memcpy(CMSG_DATA(cmsg), &afd, sizeof(int));
	}

	size = sendmsg(socketfd, &mh, MSG_DONTWAIT);
	if (size < 0) {
		if (errno == EAGAIN)
			return 0;
		plt_err("Failed to send message, err %d", -errno);
		return -errno;
	} else if (size == 0) {
		return 0;
	}
	sent++;
	plt_rep_dbg("Sent %" PRId64 " packets of size %d on socketfd %d", sent, size, socketfd);

	return size;
}

static int
open_socket_ctrl_channel(void)
{
	struct sockaddr_un un;
	int sock_fd;

	sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		plt_err("Failed to create unix socket");
		return -1;
	}

	/* Set unix socket path and bind */
	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;

	if (strlen(CNXK_ESWITCH_CTRL_MSG_SOCK_PATH) > sizeof(un.sun_path) - 1) {
		plt_err("Server socket path too long: %s", CNXK_ESWITCH_CTRL_MSG_SOCK_PATH);
		close(sock_fd);
		return -E2BIG;
	}

	if (remove(CNXK_ESWITCH_CTRL_MSG_SOCK_PATH) == -1 && errno != ENOENT) {
		plt_err("remove-%s", CNXK_ESWITCH_CTRL_MSG_SOCK_PATH);
		close(sock_fd);
		return -errno;
	}

	memset(&un, 0, sizeof(struct sockaddr_un));
	un.sun_family = AF_UNIX;
	strncpy(un.sun_path, CNXK_ESWITCH_CTRL_MSG_SOCK_PATH, sizeof(un.sun_path) - 1);

	if (bind(sock_fd, (struct sockaddr *)&un, sizeof(un)) < 0) {
		plt_err("Failed to bind %s: %s", un.sun_path, strerror(errno));
		close(sock_fd);
		return -errno;
	}

	if (listen(sock_fd, 1) < 0) {
		plt_err("Failed to listen, err %s", strerror(errno));
		close(sock_fd);
		return -errno;
	}

	plt_rep_dbg("Unix socket path %s", un.sun_path);
	return sock_fd;
}

static int
send_control_message(struct cnxk_eswitch_dev *eswitch_dev, void *buffer, uint32_t len)
{
	int sz;
	int rc = 0;

	sz = send_message_on_socket(eswitch_dev->sock_fd, buffer, len, 0);
	if (sz < 0) {
		plt_err("Error sending message, err %d", sz);
		rc = sz;
		goto done;
	}

	/* Ensuring entire message has been processed */
	if (sz != (int)len) {
		plt_err("Out of %d bytes only %d bytes sent", sz, len);
		rc = -EFAULT;
		goto done;
	}
	plt_rep_dbg("Sent %d bytes of buffer", sz);
done:
	return rc;
}

void
cnxk_rep_msg_populate_msg_end(void *buffer, uint32_t *length)
{
	cnxk_rep_msg_populate_command(buffer, length, CNXK_REP_MSG_END, 0);
}

void
cnxk_rep_msg_populate_type(void *buffer, uint32_t *length, cnxk_type_t type, uint32_t sz)
{
	uint32_t len = *length;
	cnxk_type_data_t data;

	memset(&data, 0, sizeof(cnxk_type_data_t));
	/* Prepare type data */
	data.type = type;
	data.length = sz;

	/* Populate the type data */
	rte_memcpy(RTE_PTR_ADD(buffer, len), &data, sizeof(cnxk_type_data_t));
	len += sizeof(cnxk_type_data_t);

	*length = len;
}

void
cnxk_rep_msg_populate_header(void *buffer, uint32_t *length)
{
	cnxk_header_t hdr;
	int len;

	cnxk_rep_msg_populate_type(buffer, length, CNXK_TYPE_HEADER, sizeof(cnxk_header_t));

	memset(&hdr, 0, sizeof(cnxk_header_t));
	len = *length;
	/* Prepare header data */
	hdr.signature = CTRL_MSG_SIGNATURE;

	/* Populate header data */
	rte_memcpy(RTE_PTR_ADD(buffer, len), &hdr, sizeof(cnxk_header_t));
	len += sizeof(cnxk_header_t);

	*length = len;
}

void
cnxk_rep_msg_populate_command(void *buffer, uint32_t *length, cnxk_rep_msg_t type, uint32_t size)
{
	cnxk_rep_msg_data_t msg_data;
	uint32_t len;
	uint16_t sz = sizeof(cnxk_rep_msg_data_t);

	memset(&msg_data, 0, sz);
	cnxk_rep_msg_populate_type(buffer, length, CNXK_TYPE_MSG, sz);

	len = *length;
	/* Prepare command data */
	msg_data.type = type;
	msg_data.length = size;

	/* Populate the command */
	rte_memcpy(RTE_PTR_ADD(buffer, len), &msg_data, sz);
	len += sz;

	*length = len;
}

void
cnxk_rep_msg_populate_command_meta(void *buffer, uint32_t *length, void *msg_meta, uint32_t sz,
				   cnxk_rep_msg_t msg)
{
	uint32_t len;

	cnxk_rep_msg_populate_command(buffer, length, msg, sz);

	len = *length;
	/* Populate command data */
	rte_memcpy(RTE_PTR_ADD(buffer, len), msg_meta, sz);
	len += sz;

	*length = len;
}

static int
parse_validate_header(void *msg_buf, uint32_t *buf_trav_len)
{
	cnxk_type_data_t *tdata = NULL;
	cnxk_header_t *hdr = NULL;
	void *data = NULL;
	uint16_t len = 0;

	/* Read first bytes of type data */
	data = msg_buf;
	tdata = (cnxk_type_data_t *)data;
	if (tdata->type != CNXK_TYPE_HEADER) {
		plt_err("Invalid type %d, type header expected", tdata->type);
		goto fail;
	}

	/* Get the header value */
	data = RTE_PTR_ADD(msg_buf, sizeof(cnxk_type_data_t));
	len += sizeof(cnxk_type_data_t);

	/* Validate the header */
	hdr = (cnxk_header_t *)data;
	if (hdr->signature != CTRL_MSG_SIGNATURE) {
		plt_err("Invalid signature %" PRIu64 " detected", hdr->signature);
		goto fail;
	}

	/* Update length read till point */
	len += tdata->length;

	*buf_trav_len = len;
	return 0;
fail:
	return errno;
}

static cnxk_rep_msg_data_t *
message_data_extract(void *msg_buf, uint32_t *buf_trav_len)
{
	cnxk_type_data_t *tdata = NULL;
	cnxk_rep_msg_data_t *msg = NULL;
	uint16_t len = *buf_trav_len;
	void *data;

	tdata = (cnxk_type_data_t *)RTE_PTR_ADD(msg_buf, len);
	if (tdata->type != CNXK_TYPE_MSG) {
		plt_err("Invalid type %d, type MSG expected", tdata->type);
		goto fail;
	}

	/* Get the message type */
	len += sizeof(cnxk_type_data_t);
	data = RTE_PTR_ADD(msg_buf, len);
	msg = (cnxk_rep_msg_data_t *)data;

	/* Advance to actual message data */
	len += tdata->length;
	*buf_trav_len = len;

	return msg;
fail:
	return NULL;
}

static void
process_ack_message(void *msg_buf, uint32_t *buf_trav_len, uint32_t msg_len, void *data)
{
	cnxk_rep_msg_ack_data_t *adata = (cnxk_rep_msg_ack_data_t *)data;
	uint16_t len = *buf_trav_len;
	void *buf;

	/* Get the message type data viz ack data */
	buf = RTE_PTR_ADD(msg_buf, len);
	adata->u.data = rte_zmalloc("Ack data", msg_len, 0);
	adata->size = msg_len;
	if (adata->size == sizeof(uint64_t))
		rte_memcpy(&adata->u.data, buf, msg_len);
	else
		rte_memcpy(adata->u.data, buf, msg_len);
	plt_rep_dbg("Address %p val 0x%" PRIu64 " sval %" PRId64 " msg_len %d",
		    adata->u.data, adata->u.val, adata->u.sval, msg_len);

	/* Advance length to nex message */
	len += msg_len;
	*buf_trav_len = len;
}

static int
notify_rep_dev_ready(cnxk_rep_msg_ready_data_t *rdata, void *data,
		     cnxk_rep_msg_ack_data1_t **padata)
{
	struct cnxk_eswitch_dev *eswitch_dev;
	uint64_t rep_id_arr[RTE_MAX_ETHPORTS];
	cnxk_rep_msg_ack_data1_t *adata;
	uint16_t rep_id, sz, total_sz;
	int rc, i, j = 0;

	PLT_SET_USED(data);
	eswitch_dev = cnxk_eswitch_pmd_priv();
	if (!eswitch_dev) {
		plt_err("Failed to get PF ethdev handle");
		rc = -EINVAL;
		goto fail;
	}

	memset(rep_id_arr, 0, RTE_MAX_ETHPORTS * sizeof(uint64_t));
	/* For ready state */
	if (rdata->nb_ports > eswitch_dev->repr_cnt.nb_repr_probed) {
		rc = CNXK_REP_CTRL_MSG_NACK_INV_REP_CNT;
		goto fail;
	}

	for (i = 0; i < rdata->nb_ports; i++) {
		rep_id = UINT16_MAX;
		rc = cnxk_rep_state_update(eswitch_dev, rdata->data[i], &rep_id);
		if (rc) {
			rc = CNXK_REP_CTRL_MSG_NACK_REP_STAT_UP_FAIL;
			goto fail;
		}
		if (rep_id != UINT16_MAX)
			rep_id_arr[j++] = rep_id;
	}

	/* Send Rep Id array to companian app */
	sz = j * sizeof(uint64_t);
	total_sz = sizeof(cnxk_rep_msg_ack_data1_t) + sz;
	adata = plt_zmalloc(total_sz, 0);
	rte_memcpy(adata->data, rep_id_arr, sz);
	adata->size = sz;
	*padata = adata;

	plt_rep_dbg("Installing NPC rules for Eswitch VF");
	/* Install RX VLAN rule for eswitch VF */
	if (!eswitch_dev->eswitch_vf_rules_setup) {
		rc = cnxk_eswitch_pfvf_flow_rules_install(eswitch_dev, true);
		if (rc) {
			plt_err("Failed to install rxtx rules, rc %d", rc);
			goto fail;
		}

		/* Configure TPID for Eswitch PF LFs */
		rc = roc_eswitch_nix_vlan_tpid_set(&eswitch_dev->nix, ROC_NIX_VLAN_TYPE_OUTER,
						   CNXK_ESWITCH_VLAN_TPID, true);
		if (rc) {
			plt_err("Failed to configure tpid, rc %d", rc);
			goto fail;
		}
		eswitch_dev->eswitch_vf_rules_setup = true;
	}

	return 0;
fail:
	sz = sizeof(cnxk_rep_msg_ack_data1_t) + sizeof(uint64_t);
	adata = plt_zmalloc(sz, 0);
	adata->data[0] = rc;
	adata->size = sizeof(uint64_t);
	*padata = adata;

	return rc;
}

static int
process_ready_message(void *msg_buf, uint32_t *buf_trav_len, uint32_t msg_len, void *data,
		      cnxk_rep_msg_ack_data1_t **padata)
{
	cnxk_rep_msg_ready_data_t *rdata = NULL;
	cnxk_rep_msg_ack_data1_t *adata;
	uint16_t len = *buf_trav_len;
	void *buf;
	int rc = 0, sz;

	/* Get the message type data viz ready data */
	buf = RTE_PTR_ADD(msg_buf, len);
	rdata = (cnxk_rep_msg_ready_data_t *)buf;

	plt_rep_dbg("Ready data received %d, nb_ports %d", rdata->val, rdata->nb_ports);

	/* Wait required to ensure other side ready for receiving the ack */
	usleep(CTRL_MSG_READY_WAIT_US);

	/* Update all representor about ready message */
	if (rdata->val) {
		rc = notify_rep_dev_ready(rdata, data, padata);
	} else {
		sz = sizeof(cnxk_rep_msg_ack_data1_t) + sizeof(uint64_t);
		adata = plt_zmalloc(sz, 0);
		adata->data[0] = CNXK_REP_CTRL_MSG_NACK_INV_RDY_DATA;
		adata->size = sizeof(uint64_t);
		*padata = adata;
	}

	/* Advance length to nex message */
	len += msg_len;
	*buf_trav_len = len;

	return rc;
}

static int
notify_rep_dev_exit(cnxk_rep_msg_exit_data_t *edata, void *data)
{
	struct cnxk_eswitch_dev *eswitch_dev;
	struct cnxk_rep_dev *rep_dev = NULL;
	struct rte_eth_dev *rep_eth_dev;
	int i, rc = 0;

	PLT_SET_USED(data);
	eswitch_dev = cnxk_eswitch_pmd_priv();
	if (!eswitch_dev) {
		plt_err("Failed to get PF ethdev handle");
		rc = -EINVAL;
		goto fail;
	}
	if (edata->nb_ports > eswitch_dev->repr_cnt.nb_repr_probed) {
		rc = CNXK_REP_CTRL_MSG_NACK_INV_REP_CNT;
		goto fail;
	}

	for (i = 0; i < eswitch_dev->repr_cnt.nb_repr_probed; i++) {
		rep_eth_dev = eswitch_dev->rep_info[i].rep_eth_dev;
		if (!rep_eth_dev) {
			plt_err("Failed to get rep ethdev handle");
			rc = -EINVAL;
			goto fail;
		}

		rep_dev = cnxk_rep_pmd_priv(rep_eth_dev);
		if (!rep_dev->native_repte)
			rep_dev->is_vf_active = false;
	}
	/* For Exit message */
	eswitch_dev->client_connected = false;
	return 0;
fail:
	return rc;
}

static void
process_exit_message(void *msg_buf, uint32_t *buf_trav_len, uint32_t msg_len, void *data)
{
	cnxk_rep_msg_exit_data_t *edata = NULL;
	uint16_t len = *buf_trav_len;
	void *buf;

	/* Get the message type data viz exit data */
	buf = RTE_PTR_ADD(msg_buf, len);
	edata = (cnxk_rep_msg_exit_data_t *)buf;

	plt_rep_dbg("Exit data received %d", edata->val);

	/* Update all representor about ready/exit message */
	if (edata->val)
		notify_rep_dev_exit(edata, data);

	/* Advance length to nex message */
	len += msg_len;
	*buf_trav_len = len;
}

static void
populate_ack_msg(void *buffer, uint32_t *length, cnxk_rep_msg_ack_data1_t *adata)
{
	uint32_t sz = sizeof(cnxk_rep_msg_ack_data1_t) + adata->size;
	uint32_t len;

	cnxk_rep_msg_populate_command(buffer, length, CNXK_REP_MSG_ACK, sz);

	len = *length;

	/* Populate ACK message data */
	rte_memcpy(RTE_PTR_ADD(buffer, len), adata, sz);

	len += sz;

	*length = len;
}

static int
send_ack_message(void *data, cnxk_rep_msg_ack_data1_t *adata)
{
	struct cnxk_eswitch_dev *eswitch_dev = (struct cnxk_eswitch_dev *)data;
	uint32_t len = 0, size;
	void *buffer;
	int rc = 0;

	/* Allocate memory for preparing a message */
	size = CTRL_MSG_BUFFER_SZ;
	buffer = rte_zmalloc("ACK msg", size, 0);
	if (!buffer) {
		plt_err("Failed to allocate mem");
		return -ENOMEM;
	}

	/* Prepare the ACK message */
	cnxk_rep_msg_populate_header(buffer, &len);
	populate_ack_msg(buffer, &len, adata);
	cnxk_rep_msg_populate_msg_end(buffer, &len);

	/* Length check to avoid buffer overflow */
	if (len > CTRL_MSG_BUFFER_SZ) {
		plt_err("Invalid length %d for max sized buffer %d", len, CTRL_MSG_BUFFER_SZ);
		rc = -EFAULT;
		goto done;
	}

	/* Send it to the peer */
	rc = send_control_message(eswitch_dev, buffer, len);
	if (rc)
		plt_err("Failed send ack");

done:
	return rc;
}

static int
process_message(void *msg_buf, uint32_t *buf_trav_len, void *data)
{
	cnxk_rep_msg_data_t *msg = NULL;
	cnxk_rep_msg_ack_data1_t *adata = NULL;
	bool send_ack;
	int rc = 0, sz;

	/* Get the message data */
	msg = message_data_extract(msg_buf, buf_trav_len);
	if (!msg) {
		plt_err("Failed to get message data");
		rc = -EINVAL;
		goto fail;
	}

	/* Different message type processing */
	while (msg->type != CNXK_REP_MSG_END) {
		send_ack = true;
		switch (msg->type) {
		case CNXK_REP_MSG_ACK:
			plt_rep_dbg("Received ack response");
			process_ack_message(msg_buf, buf_trav_len, msg->length, data);
			send_ack = false;
			break;
		case CNXK_REP_MSG_READY:
			plt_rep_dbg("Received ready message");
			process_ready_message(msg_buf, buf_trav_len, msg->length, data, &adata);
			adata->type = CNXK_REP_MSG_READY;
			break;
		case CNXK_REP_MSG_EXIT:
			plt_rep_dbg("Received exit message");
			process_exit_message(msg_buf, buf_trav_len, msg->length, data);
			sz = sizeof(cnxk_rep_msg_ack_data1_t) + sizeof(uint64_t);
			adata = plt_zmalloc(sz, 0);
			adata->type = CNXK_REP_MSG_EXIT;
			adata->data[0] = 0;
			adata->size = sizeof(uint64_t);
			break;
		default:
			send_ack = false;
			plt_err("Invalid message type: %d", msg->type);
			rc = -EINVAL;
		};

		/* Send ACK */
		if (send_ack)
			send_ack_message(data, adata);

		/* Advance to next message */
		msg = message_data_extract(msg_buf, buf_trav_len);
		if (!msg) {
			plt_err("Failed to get message data");
			rc = -EINVAL;
			goto fail;
		}
	}

	return 0;
fail:
	return rc;
}

static int
process_control_message(void *msg_buf, void *data, size_t sz)
{
	uint32_t buf_trav_len = 0;
	int rc;

	/* Validate the validity of the received message */
	parse_validate_header(msg_buf, &buf_trav_len);

	/* Detect message and process */
	rc = process_message(msg_buf, &buf_trav_len, data);
	if (rc) {
		plt_err("Failed to process message");
		goto fail;
	}

	/* Ensuring entire message has been processed */
	if (sz != buf_trav_len) {
		plt_err("Out of %" PRId64 " bytes %d bytes of msg_buf processed", sz, buf_trav_len);
		rc = -EFAULT;
		goto fail;
	}

	return 0;
fail:
	return rc;
}

static int
receive_control_msg_resp(struct cnxk_eswitch_dev *eswitch_dev, void *data)
{
	uint32_t wait_us = CTRL_MSG_RCV_TIMEOUT_MS * 1000;
	uint32_t timeout = 0, sleep = 1;
	int sz = 0;
	int rc = -1;
	uint32_t len = BUFSIZ;
	void *msg_buf;

	msg_buf = plt_zmalloc(len, 0);

	do {
		sz = receive_control_message(eswitch_dev->sock_fd, msg_buf, len);
		if (sz != 0)
			break;

		/* Timeout after CTRL_MSG_RCV_TIMEOUT_MS */
		if (timeout >= wait_us) {
			plt_err("Control message wait timedout");
			return -ETIMEDOUT;
		}

		plt_delay_us(sleep);
		timeout += sleep;
	} while ((sz == 0) || (timeout < wait_us));

	if (sz > 0) {
		plt_rep_dbg("Received %d sized response packet", sz);
		rc = process_control_message(msg_buf, data, sz);
		plt_free(msg_buf);
	}

	return rc;
}

int
cnxk_rep_msg_send_process(struct cnxk_rep_dev *rep_dev, void *buffer, uint32_t len,
			  cnxk_rep_msg_ack_data_t *adata)
{
	struct cnxk_eswitch_dev *eswitch_dev;
	int rc = 0;

	eswitch_dev = rep_dev->parent_dev;
	if (!eswitch_dev) {
		plt_err("Failed to get parent eswitch handle");
		rc = -1;
		goto fail;
	}

	plt_spinlock_lock(&eswitch_dev->rep_lock);
	rc = send_control_message(eswitch_dev, buffer, len);
	if (rc) {
		plt_err("Failed to send the message, err %d", rc);
		goto free;
	}

	/* Get response of the command sent */
	rc = receive_control_msg_resp(eswitch_dev, adata);
	if (rc) {
		plt_err("Failed to receive the response, err %d", rc);
		goto free;
	}
	plt_spinlock_unlock(&eswitch_dev->rep_lock);

	return 0;
free:
	plt_spinlock_unlock(&eswitch_dev->rep_lock);
fail:
	return rc;
}

static void
poll_for_control_msg(void *data)
{
	struct cnxk_eswitch_dev *eswitch_dev = (struct cnxk_eswitch_dev *)data;
	uint32_t len = BUFSIZ;
	int sz = 0;
	void *msg_buf;

	while (eswitch_dev->client_connected) {
		msg_buf = plt_zmalloc(len, 0);
		do {
			plt_spinlock_lock(&eswitch_dev->rep_lock);
			sz = receive_control_message(eswitch_dev->sock_fd, msg_buf, len);
			plt_spinlock_unlock(&eswitch_dev->rep_lock);
			if (sz != 0)
				break;
			plt_delay_us(2000);
		} while (sz == 0);

		if (sz > 0) {
			plt_rep_dbg("Received new %d bytes control message", sz);
			plt_spinlock_lock(&eswitch_dev->rep_lock);
			process_control_message(msg_buf, data, sz);
			plt_spinlock_unlock(&eswitch_dev->rep_lock);
			plt_free(msg_buf);
		}
	}
	plt_rep_dbg("Exiting poll for control message loop");
}

static uint32_t
rep_ctrl_msg_thread_main(void *arg)
{
	struct cnxk_eswitch_dev *eswitch_dev = (struct cnxk_eswitch_dev *)arg;
	struct sockaddr_un client;
	int addr_len;
	int ssock_fd;
	int sock_fd;

	ssock_fd = open_socket_ctrl_channel();
	if (ssock_fd < 0) {
		plt_err("Failed to open socket for ctrl channel, err %d", ssock_fd);
		return UINT32_MAX;
	}

	addr_len = sizeof(client);
	while (eswitch_dev->start_ctrl_msg_thrd) {
		/* Accept client connection until the thread is running */
		sock_fd = accept(ssock_fd, (struct sockaddr *)&client, (socklen_t *)&addr_len);
		if (sock_fd < 0) {
			plt_err("Failed to accept connection request on socket fd %d", ssock_fd);
			break;
		}

		plt_rep_dbg("Client %s: Connection request accepted.", client.sun_path);
		eswitch_dev->sock_fd = sock_fd;
		if (eswitch_dev->start_ctrl_msg_thrd) {
			eswitch_dev->client_connected = true;
			poll_for_control_msg(eswitch_dev);
		}
		eswitch_dev->sock_fd = -1;
		close(sock_fd);
	}

	/* Closing the opened socket */
	close_socket(ssock_fd);
	plt_rep_dbg("Exiting representor ctrl thread");

	return 0;
}

int
cnxk_rep_msg_control_thread_launch(struct cnxk_eswitch_dev *eswitch_dev)
{
	char name[CTRL_MSG_THRD_NAME_LEN];
	int rc = 0;

	rte_strscpy(name, "rep_ctrl_msg_hndlr", CTRL_MSG_THRD_NAME_LEN);
	eswitch_dev->start_ctrl_msg_thrd = true;
	rc = rte_thread_create_internal_control(&eswitch_dev->rep_ctrl_msg_thread, name,
						rep_ctrl_msg_thread_main, eswitch_dev);
	if (rc)
		plt_err("Failed to create rep control message handling");

	return rc;
}
