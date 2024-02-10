/*
 * Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the Mellanox Technologies Ltd nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#ifndef DOCA_CC_HELPER_H_
#define DOCA_CC_HELPER_H_

#include <stdint.h>
#include <iostream>

#include "doca_comm_channel.h"
#include <doca_cc.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_error.h>
#include <doca_log.h>
#include <doca_pe.h>
#include <doca_buf_inventory.h>
#include <doca_cc_consumer.h>
#include <doca_cc_producer.h>
#include <doca_mmap.h>

#include "os_abstract.h"

#define MSG_SIZE 4080
#define MAX_BUFS 1
#define CC_DATA_PATH_LOG_TASK_NUM 10	/* Maximum amount of CC consumer and producer task number */
#define PCI_ADDR_LEN 8
#define CC_MAX_QUEUE_SIZE 1024			/* Maximum amount of message in queue */
#define CC_REC_QUEUE_SIZE 10			/* Maximum amount of message in queue */
#define CC_SEND_TASK_NUM 10				/* Maximum amount of CC send task number */
#define NANOS_10_X_1000 (10 * 1000)

#define log_dbg(log_fmt, ...) 																	\
        printf("Doca CC" ": " log_fmt "\n", ##__VA_ARGS__);
#define DOCA_LOG_INFO(format, ...) log_dbg(format, ##__VA_ARGS__)
#define DOCA_LOG_ERR(format, ...) log_dbg(format, ##__VA_ARGS__)
#define DOCA_LOG_DBG(format, ...) log_dbg(format, ##__VA_ARGS__)

enum cc_state {
	CC_CONNECTION_IN_PROGRESS,
	CC_CONNECTED
};
enum cc_fifo_connection_state {
	CC_FIFO_CONNECTION_IN_PROGRESS,
	CC_FIFO_CONNECTED
};

struct cc_local_mem_bufs {
	void *mem;						/* Memory address for DOCA buf mmap */
	struct doca_mmap *mmap;			/* DOCA mmap object */
	struct doca_buf_inventory *buf_inv;	/* DOCA buf inventory object */
	int msg_size;							/**<msg size for buffer */
};

struct cc_ctx_fifo {
	struct doca_cc_consumer *consumer;		/**< CC consumer object */
	struct cc_local_mem_bufs consumer_mem;	/**< Mmap and DOCA buf objects for consumer */
	struct doca_cc_producer *producer;		/**< CC producer object */
	struct cc_local_mem_bufs producer_mem;	/**< Mmap and DOCA buf objects for producer */
	uint32_t remote_consumer_id;			/**< Consumer ID on the peer side */
	struct doca_pe *pe;						/**< Progress Engine for */
	struct doca_pe *pe_underload;						/**< Progress Engine for */
	bool underload_mode;					/**< For using different callback>*/
	enum cc_fifo_connection_state fifo_connection_state;		 	/**< Holding state for fast path connection >*/
	struct doca_buf *doca_buf_consumer;
	struct doca_buf *doca_buf_producer;
	struct doca_cc_consumer_post_recv_task *consumer_task;
	struct doca_cc_producer_send_task *producer_task;
	struct doca_task *consumer_task_obj;
	struct doca_task *producer_task_obj;
	bool task_submitted;							/**< Indicated if task was already submitted*/

};

struct cc_ctx {
	struct doca_dev *hw_dev;				/**< Doca Device used per PCI address > */
	struct doca_cc_connection *connection;	/**< Connection object used for pairing a connection >*/
	uint32_t num_connected_clients;			/**< Number of currently connected clients >*/
	uint8_t *recv_buffer;					/**< Pointer to recv buffer >*/
	int buf_size;							/**< Buffer size of recv buffer >*/
	bool recv_flag;							/**< flag indicates when message received >*/
	int fd;									/**< File Descriptor >*/
	os_mutex_t lock;						/**< For underload mode only>*/
	os_cond_t cond;							/**< For underload mode only>*/
	enum cc_state state;		 			/**< Holding state of client connection >*/
	bool fast_path;							/**< Indicated for using fast data path*/
	struct cc_ctx_fifo ctx_fifo;			/**< Data path objects */
};

struct cc_ctx_server {
	struct cc_ctx ctx;						/**< Base common ctx >*/
	struct doca_dev_rep *rep_dev;			/**< Device representor >*/
	struct doca_cc_server *server;			/**< Server object >*/
};

struct cc_ctx_client {
	struct cc_ctx ctx;						/**< Base common ctx >*/
	struct doca_cc_client *client;			/**< Client object >*/
};

struct priv_doca_pci_bdf {
        #define PCI_FUNCTION_MAX_VALUE 8
        #define PCI_DEVICE_MAX_VALUE 32
        #define PCI_BUS_MAX_VALUE 256
        union {
                uint16_t raw;
                struct {
                        uint16_t function : 3;
                        uint16_t device : 5;
                        uint16_t bus : 8;
                };
        };
};

/************** General ******************/
static doca_error_t cc_init_producer(struct cc_ctx *ctx);
static doca_error_t cc_init_consumer(struct cc_ctx *ctx);
static doca_error_t
cc_parse_pci_addr(char const *pci_addr, struct priv_doca_pci_bdf *out_bdf)
{
        unsigned int bus_bitmask = 0xFFFFFF00;
        unsigned int dev_bitmask = 0xFFFFFFE0;
        unsigned int func_bitmask = 0xFFFFFFF8;
        uint32_t tmpu;
        char tmps[4];

        if (pci_addr == NULL || strlen(pci_addr) != 7 || pci_addr[2] != ':' || pci_addr[5] != '.')
                return DOCA_ERROR_INVALID_VALUE;

        tmps[0] = pci_addr[0];
        tmps[1] = pci_addr[1];
        tmps[2] = '\0';
        tmpu = strtoul(tmps, NULL, 16);
        if ((tmpu & bus_bitmask) != 0)
                return DOCA_ERROR_INVALID_VALUE;

        tmps[0] = pci_addr[3];
        tmps[1] = pci_addr[4];
        tmps[2] = '\0';
        tmpu = strtoul(tmps, NULL, 16);
        if ((tmpu & dev_bitmask) != 0)
                return DOCA_ERROR_INVALID_VALUE;

        tmps[0] = pci_addr[6];
        tmps[1] = '\0';
        tmpu = strtoul(tmps, NULL, 16);
        if ((tmpu & func_bitmask) != 0)
                return DOCA_ERROR_INVALID_VALUE;

        return DOCA_SUCCESS;
}

typedef doca_error_t (*jobs_check)(struct doca_devinfo *);

static doca_error_t
cc_open_doca_device_with_pci(const struct priv_doca_pci_bdf *value, jobs_check func, struct doca_dev **retval)
{
        struct doca_devinfo **dev_list;
        uint32_t nb_devs;
        char pci_buf[DOCA_DEVINFO_REP_PCI_ADDR_SIZE] = {};

        doca_error_t res;
        size_t i;

        /* Set default return value */
        *retval = NULL;

        res = doca_devinfo_create_list(&dev_list, &nb_devs);
        if (res != DOCA_SUCCESS) {
                DOCA_LOG_ERR("Failed to load doca devices list. Doca_error value: %d", res);
                return res;
        }

        /* Search */
        for (i = 0; i < nb_devs; i++) {
                res = doca_devinfo_get_pci_addr_str(dev_list[i], pci_buf);
                if (res == DOCA_SUCCESS) {
                        /* If any special capabilities are needed */
                        if (func != NULL && func(dev_list[i]) != DOCA_SUCCESS)
                                continue;

                        /* if device can be opened */
                        res = doca_dev_open(dev_list[i], retval);
                        if (res == DOCA_SUCCESS) {
                                doca_devinfo_destroy_list(dev_list);
                                return res;
                        }
                }
        }

        DOCA_LOG_ERR("Matching device not found.");
        res = DOCA_ERROR_NOT_FOUND;

        doca_devinfo_destroy_list(dev_list);
        return res;
}

static doca_error_t
cc_open_doca_device_rep_with_pci(struct doca_dev *local, enum doca_devinfo_rep_filter filter, struct priv_doca_pci_bdf *pci_bdf,
                              struct doca_dev_rep **retval)
{
        uint32_t nb_rdevs = 0;
        struct doca_devinfo_rep **rep_dev_list = NULL;
        char pci_buf[DOCA_DEVINFO_REP_PCI_ADDR_SIZE] = {};

        doca_error_t result;
        size_t i;

        *retval = NULL;

        /* Search */
        result = doca_devinfo_rep_create_list(local, filter, &rep_dev_list, &nb_rdevs);
        if (result != DOCA_SUCCESS) {
                DOCA_LOG_ERR(
                        "Failed to create devinfo representors list. Representor devices are available only on DPU, do not run on Host.");
                return DOCA_ERROR_INVALID_VALUE;
        }

        for (i = 0; i < nb_rdevs; i++) {
                result = doca_devinfo_rep_get_pci_addr_str(rep_dev_list[i], pci_buf);
                if (result == DOCA_SUCCESS &&
                    doca_dev_rep_open(rep_dev_list[i], retval) == DOCA_SUCCESS) {
                        doca_devinfo_rep_destroy_list(rep_dev_list);
                        return DOCA_SUCCESS;
                }
        }

        DOCA_LOG_ERR("Matching device not found.");
        doca_devinfo_rep_destroy_list(rep_dev_list);
        return DOCA_ERROR_NOT_FOUND;
}

static doca_error_t
cc_init_local_mem_bufs(struct cc_local_mem_bufs *local_mem, struct cc_ctx *ctx)
{
	doca_error_t result;

	result = doca_buf_inventory_create(MAX_BUFS, &(local_mem->buf_inv));
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to create inventory: %s", doca_error_get_descr(result));
	}

	result = doca_buf_inventory_start(local_mem->buf_inv);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to start inventory: %s", doca_error_get_descr(result));
	}

	result = doca_mmap_create(&local_mem->mmap);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to create mmap: %s", doca_error_get_descr(result));
	}

	result = doca_mmap_add_dev(local_mem->mmap, ctx->hw_dev);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to add device to mmap: %s", doca_error_get_descr(result));
	}

	result = doca_mmap_set_permissions(local_mem->mmap, DOCA_ACCESS_FLAG_PCI_READ_WRITE);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to set permission to mmap: %s", doca_error_get_descr(result));
	}

	// set here sockperf buf as local->mem
	//result = doca_mmap_set_memrange(local_mem->mmap, local_mem->mem, sizeof(uint8_t) * ctx->ctx_fifo.msg_size * 2);
	result = doca_mmap_set_memrange(local_mem->mmap, local_mem->mem, sizeof(uint8_t) * local_mem->msg_size * 2);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to set memrange to mmap: %s", doca_error_get_descr(result));
	}

	result = doca_mmap_start(local_mem->mmap);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to start mmap: %s", doca_error_get_descr(result));
	}

	return DOCA_SUCCESS;
}

static doca_error_t
cc_init_doca_consumer_task(struct cc_local_mem_bufs *local_mem, struct cc_ctx_fifo *ctx_fifo)
{
	doca_error_t result;
	result = doca_buf_inventory_buf_get_by_addr(local_mem->buf_inv, local_mem->mmap, local_mem->mem,
												local_mem->msg_size, &ctx_fifo->doca_buf_consumer);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to get doca buf: %s", doca_error_get_descr(result));
	}
	result = doca_cc_consumer_post_recv_task_alloc_init(ctx_fifo->consumer, ctx_fifo->doca_buf_consumer, &ctx_fifo->consumer_task);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to allocate consumer task : %s", doca_error_get_descr(result));
	}
	ctx_fifo->consumer_task_obj = doca_cc_consumer_post_recv_task_as_task(ctx_fifo->consumer_task);
	return result;

}

static doca_error_t
cc_init_doca_producer_task(struct cc_local_mem_bufs *local_mem, struct cc_ctx_fifo *ctx_fifo)
{
	doca_error_t result;
	result = doca_buf_inventory_buf_get_by_addr(local_mem->buf_inv, local_mem->mmap, local_mem->mem,
												local_mem->msg_size, &ctx_fifo->doca_buf_producer);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to get doca buf: %s", doca_error_get_descr(result));
	}
	result = doca_cc_producer_send_task_alloc_init(ctx_fifo->producer, ctx_fifo->doca_buf_producer,
												ctx_fifo->remote_consumer_id ,&ctx_fifo->producer_task);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to allocate consumer task : %s", doca_error_get_descr(result));
	}
	ctx_fifo->producer_task_obj = doca_cc_producer_send_task_as_task(ctx_fifo->producer_task);
	return result;

}
/************** SERVER ******************/

/**
 * Callback for send task successfull completion
 *
 * @task [in]: Send task object
 * @task_user_data [in]: User data for task
 * @user_data [in]: User data for context
 */
static void
cc_server_send_task_completion_callback(struct doca_cc_send_task *task, union doca_data task_user_data,
			      union doca_data user_data)
{
	/* These arguments are not in use */
	(void)user_data;
	(void)task_user_data;

	// DOCA_LOG_INFO("Task sent successfully");

	doca_task_free(doca_cc_send_task_as_task(task));
}

/**
 * Callback for send task completion with error
 *
 * @task [in]: Send task object
 * @task_user_data [in]: User data for task
 * @user_data [in]: User data for context
 */
static void
cc_server_send_task_completion_err_callback(struct doca_cc_send_task *task, union doca_data task_user_data,
				  union doca_data user_data)
{
	struct cc_ctx_server *ctx_server = (struct cc_ctx_server *)user_data.ptr;
	doca_error_t result;

	/* This argument is not in use */
	(void)task_user_data;

	result = doca_task_get_status(doca_cc_send_task_as_task(task));
	DOCA_LOG_ERR("[fd=%d] Message failed to send with error = %s", ctx_server->ctx.fd , doca_error_get_name(result));

	doca_task_free(doca_cc_send_task_as_task(task));
	(void)doca_ctx_stop(doca_cc_server_as_ctx(ctx_server->server));
}

/**
 * Callback for message recv event
 *
 * @event [in]: Recv event object
 * @recv_buffer [in]: Message buffer
 * @msg_len [in]: Message len
 * @cc_connection [in]: Connection the message was received on
 */
static void
cc_server_message_recv_callback(struct doca_cc_event_msg_recv *event, uint8_t *recv_buffer, size_t msg_len,
		      struct doca_cc_connection *cc_connection)
{
	union doca_data user_data;
	struct doca_cc_server *cc_server;
	doca_error_t result;

	/* This argument is not in use */
	(void)event;

	cc_server = doca_cc_server_get_server_ctx(cc_connection);

	result = doca_ctx_get_user_data(doca_cc_server_as_ctx(cc_server), &user_data);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to get user data from ctx with error = %s", doca_error_get_name(result));
		return;
	}

	/* Save the connection that the ping was sent over for sending the response */
	struct cc_ctx_server *ctx_server = (struct cc_ctx_server *)user_data.ptr;
	ctx_server->ctx.connection = cc_connection;

	// DOCA_LOG_INFO("Message received: '%d, pointer is %p", (int)msg_len, recv_buffer);

	memcpy(ctx_server->ctx.recv_buffer, recv_buffer, msg_len);
	ctx_server->ctx.buf_size = (int)msg_len;
	ctx_server->ctx.recv_flag = true;
}

/**
 * Callback for consumer post recv task successfull completion
 *
 * @task [in]: Recv task object
 * @task_user_data [in]: User data for task
 * @ctx_user_data [in]: User data for context
 */
static void
cc_consumer_recv_task_completion_callback(struct doca_cc_consumer_post_recv_task *task, union doca_data task_user_data,
					union doca_data user_data)
{
	size_t recv_msg_len;
	void *recv_msg;
	struct doca_buf *buf;
	doca_error_t result;

	(void)task_user_data;
	struct cc_ctx *ctx = (struct cc_ctx *)user_data.ptr;

	buf = doca_cc_consumer_post_recv_task_get_buf(task);

	result = doca_buf_get_data(buf, &recv_msg);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to get data address from DOCA buf with error = %s", doca_error_get_name(result));
	}

	result = doca_buf_get_data_len(buf, &recv_msg_len);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to get data length from DOCA buf with error = %s", doca_error_get_name(result));
	}

	ctx->buf_size = (int)recv_msg_len;
	ctx->recv_flag = true;

	// DOCA_LOG_INFO("Message received: '%.*s'", (int)recv_msg_len, (char *)recv_msg);
}

/**
 * Callback for consumer post recv task completion with error
 *
 * @task [in]: Send task object
 * @task_user_data [in]: User data for task
 * @ctx_user_data [in]: User data for context
 */
static void
cc_consumer_recv_task_completion_err_callback(struct doca_cc_consumer_post_recv_task *task, union doca_data task_user_data,
					   union doca_data user_data)
{
	struct doca_buf *buf;
	doca_error_t result;

	(void)task_user_data;

	struct cc_ctx_server *ctx_server = (struct cc_ctx_server *)user_data.ptr;
	result = doca_task_get_status(doca_cc_consumer_post_recv_task_as_task(task));
	DOCA_LOG_ERR("Consumer failed to recv message with error = %s", doca_error_get_name(result));

	buf = doca_cc_consumer_post_recv_task_get_buf(task);
	(void)doca_buf_dec_refcount(buf, NULL);
	doca_task_free(doca_cc_consumer_post_recv_task_as_task(task));
	(void)doca_ctx_stop(doca_cc_consumer_as_ctx(ctx_server->ctx.ctx_fifo.consumer));
}

static doca_error_t
cc_init_consumer(struct cc_ctx *ctx)
{
	doca_error_t result;
	doca_data user_data;
	struct doca_ctx *doca_ctx;
	struct cc_local_mem_bufs *local_consumer_mem = &(ctx->ctx_fifo.consumer_mem);

	result = doca_cc_consumer_create(ctx->connection, local_consumer_mem->mmap, &(ctx->ctx_fifo.consumer));
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create consumer with error = %s", doca_error_get_name(result));
		return result;
	}
	doca_ctx = doca_cc_consumer_as_ctx(ctx->ctx_fifo.consumer);
	if (ctx->ctx_fifo.underload_mode) {
		result = doca_pe_connect_ctx(ctx->ctx_fifo.pe_underload, doca_ctx);
	} else {
		result = doca_pe_connect_ctx(ctx->ctx_fifo.pe, doca_ctx);
	}
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed adding pe context to server with error = %s", doca_error_get_name(result));
	}
	result = doca_cc_consumer_post_recv_task_set_conf(ctx->ctx_fifo.consumer, cc_consumer_recv_task_completion_callback,
							  cc_consumer_recv_task_completion_err_callback, CC_DATA_PATH_LOG_TASK_NUM);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed setting consumer recv task cbs with error = %s", doca_error_get_name(result));
		return result;
	}
	user_data.ptr = (void*) ctx;
	result = doca_ctx_set_user_data(doca_ctx, user_data);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed setting consumer user data with error = %s", doca_error_get_name(result));
		return result;
	}
	result = doca_ctx_start(doca_ctx);
	if (result != DOCA_ERROR_IN_PROGRESS) {
		DOCA_LOG_ERR("Failed to start consumer context with error = %s", doca_error_get_name(result));
	}
	return DOCA_SUCCESS;
}

/**
 * Callback for connection event
 *
 * @event [in]: Connection event object
 * @cc_conn [in]: Connection object
 * @change_success [in]: Whether the connection was successful or not
 */
static void
cc_server_connection_event_callback(struct doca_cc_event_connection_status_changed *event,
				  struct doca_cc_connection *cc_conn, bool change_success)
{
	union doca_data user_data;
	struct doca_cc_server *cc_server;
	doca_error_t result;

	/* This argument is not in use */
	(void)event;

	cc_server = doca_cc_server_get_server_ctx(cc_conn);

	result = doca_ctx_get_user_data(doca_cc_server_as_ctx(cc_server), &user_data);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to get user data from ctx with error = %s", doca_error_get_name(result));
		return;
	}

	/* Update number of connected clients in case of successful connection */
	struct cc_ctx_server *ctx_server = (struct cc_ctx_server *)user_data.ptr;
	if (!change_success) {
		DOCA_LOG_ERR("[fd=%d] Failed connection received", ctx_server->ctx.fd);
		return;
	}

	ctx_server->ctx.num_connected_clients++;
	ctx_server->ctx.connection = cc_conn;

	if (ctx_server->ctx.fast_path) {
		/* Init a cc consumer */
		result = cc_init_consumer(&ctx_server->ctx);
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("[fd=%d] Failed to init a consumer with error = %s", ctx_server->ctx.fd, doca_error_get_name(result));
		}
		/* Init a cc producer */
		result = cc_init_producer(&ctx_server->ctx);
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("[fd=%d] Failed to init a producer with error = %s", ctx_server->ctx.fd, doca_error_get_name(result));
		}
		DOCA_LOG_INFO("Consumer & Producer were created successfully");
	}
	DOCA_LOG_INFO("[fd=%d] New client connected to server", ctx_server->ctx.fd);
}

/**
 * Callback for disconnection event
 *
 * @event [in]: Connection event object
 * @cc_conn [in]: Connection object
 * @change_success [in]: Whether the disconnection was successful or not
 */
static void
cc_server_disconnection_event_callback(struct doca_cc_event_connection_status_changed *event,
				  struct doca_cc_connection *cc_conn, bool change_success)
{
	union doca_data user_data;
	struct doca_cc_server *cc_server;
	doca_error_t result;

	/* These arguments are not in use */
	(void)event;
	(void)change_success;

	cc_server = doca_cc_server_get_server_ctx(cc_conn);

	result = doca_ctx_get_user_data(doca_cc_server_as_ctx(cc_server), &user_data);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to get user data from ctx with error = %s", doca_error_get_name(result));
		return;
	}

	/* Update number of connected clients in case of disconnection, Currently disconnection only happens if server
	 * sent a message to a client which already stopped.
	 */
	struct cc_ctx_server *ctx_server = (struct cc_ctx_server *)user_data.ptr;
	ctx_server->ctx.num_connected_clients--;
	DOCA_LOG_INFO("[fd=%d] client was disconnected from server", ctx_server->ctx.fd);
}

/**
 * Callback triggered whenever CC server context state changes
 *
 * @user_data [in]: User data associated with the CC server context. Will hold struct cc_ctrl_path_objects *
 * @ctx [in]: The CC server context that had a state change
 * @prev_state [in]: Previous context state
 * @next_state [in]: Next context state (context is already in this state when the callback is called)
 */
static void
cc_server_state_changed_callback(const union doca_data user_data, struct doca_ctx *ctx, enum doca_ctx_states prev_state,
				enum doca_ctx_states next_state)
{
	(void)ctx;
	(void)prev_state;

	struct cc_ctx_server *ctx_server = (struct cc_ctx_server *)user_data.ptr;

	switch (next_state) {
	case DOCA_CTX_STATE_IDLE:
		DOCA_LOG_INFO("[fd=%d] CC server context has been stopped.", ctx_server->ctx.fd);
		break;
	case DOCA_CTX_STATE_STARTING:
		/**
		 * The context is in starting state, this is unexpected for CC server.
		 */
		DOCA_LOG_ERR("[fd=%d] CC server context entered into starting state. Unexpected transition", ctx_server->ctx.fd);
		break;
	case DOCA_CTX_STATE_RUNNING:
		DOCA_LOG_INFO("[fd=%d] CC server context is running. Waiting for clients to connect", ctx_server->ctx.fd);
		break;
	case DOCA_CTX_STATE_STOPPING:
		/**
		 * The context is in stopping, this can happen when fatal error encountered or when stopping context.
		 * doca_pe_progress() will cause all tasks to be flushed, and finally transition state to idle
		 */
		DOCA_LOG_INFO("[fd=%d] CC server context entered into stopping state. Terminating connections with clients", ctx_server->ctx.fd);
		break;
	default:
		break;
	}
}

/**
 * Callback for new consumer arrival event
 *
 * @event [in]: New remote consumer event object
 * @cc_connection [in]: The connection related to the consumer
 * @id [in]: The ID of the new remote consumer
 */
static void
cc_server_new_consumer_callback(struct doca_cc_event_consumer *event, struct doca_cc_connection *cc_connection, uint32_t id)
{
	union doca_data user_data;
	struct doca_cc_server *cc_server;
	doca_error_t result;

	/* This argument is not in use */
	(void)event;

	cc_server = doca_cc_server_get_server_ctx(cc_connection);

	result = doca_ctx_get_user_data(doca_cc_server_as_ctx(cc_server), &user_data);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to get user data from ctx with error = %s", doca_error_get_name(result));
		return;
	}
	struct cc_ctx_server *ctx_server = (struct cc_ctx_server *)user_data.ptr;
	ctx_server->ctx.ctx_fifo.remote_consumer_id = id;
	DOCA_LOG_INFO("[fd=%d] Got a new remote consumer with ID = [%d]",ctx_server->ctx.fd, id);
	ctx_server->ctx.ctx_fifo.fifo_connection_state = CC_FIFO_CONNECTED;

}

/**
 * Callback for expired consumer arrival event
 *
 * @event [in]: Expired remote consumer event object
 * @cc_connection [in]: The connection related to the consumer
 * @id [in]: The ID of the expired remote consumer
 */
static void
cc_server_expired_consumer_callback(struct doca_cc_event_consumer *event, struct doca_cc_connection *cc_connection, uint32_t id)
{
	/* These arguments are not in use */
	(void)event;
	(void)cc_connection;
	(void)id;
}


static doca_error_t
cc_doca_server_set_params(struct cc_ctx_server *ctx_server)
{
	struct doca_ctx *ctx;
	doca_error_t result;
	union doca_data user_data;

	ctx = doca_cc_server_as_ctx(ctx_server->server);
	result = doca_ctx_set_state_changed_cb(ctx, cc_server_state_changed_callback);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed setting state change callback with error = %s", doca_error_get_name(result));
	}
	DOCA_LOG_DBG("doca_ctx_set_state_changed_cb succeeded");

	result = doca_cc_server_send_task_set_conf(ctx_server->server, cc_server_send_task_completion_callback,
							cc_server_send_task_completion_err_callback, CC_SEND_TASK_NUM);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed setting send task cbs with error = %s", doca_error_get_name(result));
	}
	DOCA_LOG_DBG("doca_cc_server_send_task_set_conf succeeded");

	result = doca_cc_server_event_msg_recv_register(ctx_server->server, cc_server_message_recv_callback);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed adding message recv event cb with error = %s", doca_error_get_name(result));
	}
	DOCA_LOG_DBG("doca_cc_server_event_msg_recv_register succeeded");

	result = doca_cc_server_event_connection_register(ctx_server->server, cc_server_connection_event_callback,
							cc_server_disconnection_event_callback);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed adding connection event cbs with error = %s", doca_error_get_name(result));
	}
	DOCA_LOG_DBG("doca_cc_server_event_connection_register succeeded");

	/* Set server properties */
	result = doca_cc_server_set_max_msg_size(ctx_server->server, MSG_SIZE);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set msg size property with error = %s", doca_error_get_name(result));
	}

	result = doca_cc_server_set_recv_queue_size(ctx_server->server, CC_REC_QUEUE_SIZE);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set msg size property with error = %s", doca_error_get_name(result));
	}
	if (ctx_server->ctx.fast_path) { // Fast path option
		result = doca_cc_server_event_consumer_register(ctx_server->server, cc_server_new_consumer_callback,
				cc_server_expired_consumer_callback);
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Failed adding consumer event cb with error = %s", doca_error_get_name(result));
		}
		struct cc_local_mem_bufs *local_consumer_mem = &(ctx_server->ctx.ctx_fifo.consumer_mem);
		local_consumer_mem->mem = ctx_server->ctx.recv_buffer;
		result = cc_init_local_mem_bufs(local_consumer_mem, &ctx_server->ctx);
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Failed to init consumer memory with error = %s", doca_error_get_name(result));
			return result;
		}
		DOCA_LOG_DBG("Init consumer memory succeeded");

	}
	user_data.ptr = (void *)ctx_server;

	result = doca_ctx_set_user_data(ctx, user_data);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set ctx user data with error = %s", doca_error_get_name(result));
	}

	result = doca_ctx_start(ctx);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to start server context with error = %s", doca_error_get_name(result));
	}
	DOCA_LOG_INFO("[fd=%d] server properties setters succeeded", ctx_server->ctx.fd);
	return result;

}

/************** CLIENT ******************/

/**
 * Callback for send task successfull completion
 *
 * @task [in]: Send task object
 * @task_user_data [in]: User data for task
 * @user_data [in]: User data for context
 */
static void
cc_client_send_task_completion_callback(struct doca_cc_send_task *task, union doca_data task_user_data,
			      union doca_data user_data)
{
	/* These arguments are not in use */
	(void)user_data;
	(void)task_user_data;

	// DOCA_LOG_INFO("Task sent successfully");

	doca_task_free(doca_cc_send_task_as_task(task));
}

/**
 * Callback for send task completion with error
 *
 * @task [in]: Send task object
 * @task_user_data [in]: User data for task
 * @user_data [in]: User data for context
 */

static void
cc_client_send_task_completion_err_callback(struct doca_cc_send_task *task, union doca_data task_user_data,
				  union doca_data user_data)
{
	struct cc_ctx_client *cc_client = (struct cc_ctx_client *)user_data.ptr;
	doca_error_t result;

	/* This argument is not in use */
	(void)task_user_data;

	result = doca_task_get_status(doca_cc_send_task_as_task(task));
	DOCA_LOG_ERR("[fd=%d] Message failed to send with error = %s", cc_client->ctx.fd, doca_error_get_name(result));

	doca_task_free(doca_cc_send_task_as_task(task));
	(void)doca_ctx_stop(doca_cc_client_as_ctx(cc_client->client));
}

/**
 * Callback for message recv event
 *
 * @event [in]: Recv event object
 * @recv_buffer [in]: Message buffer
 * @msg_len [in]: Message len
 * @cc_connection [in]: Connection the message was received on
 */
static void
cc_client_message_recv_callback(struct doca_cc_event_msg_recv *event, uint8_t *recv_buffer, size_t msg_len,
		      struct doca_cc_connection *cc_connection)
{
	union doca_data user_data = doca_cc_connection_get_user_data(cc_connection);
	struct cc_ctx_client *cc_client = (struct cc_ctx_client *)user_data.ptr;

	/* This argument is not in use */
	(void)event;

	// DOCA_LOG_INFO("[fd=%d] Message received: '%d", cc_client->ctx.fd, (int)msg_len);
	memcpy(cc_client->ctx.recv_buffer, recv_buffer, msg_len);
	cc_client->ctx.buf_size = (int)msg_len;
	cc_client->ctx.recv_flag = true;
}

/**
 * Callback for message recv event
 *
 * @event [in]: Recv event object
 * @recv_buffer [in]: Message buffer
 * @msg_len [in]: Message len
 * @cc_connection [in]: Connection the message was received on
 */
static void
cc_client_message_UL_recv_callback(struct doca_cc_event_msg_recv *event, uint8_t *recv_buffer, size_t msg_len,
		      struct doca_cc_connection *cc_connection)
{
	union doca_data user_data = doca_cc_connection_get_user_data(cc_connection);
	struct cc_ctx_client *cc_client = (struct cc_ctx_client *)user_data.ptr;

	struct timespec ts = {
		.tv_sec = 0,
		.tv_nsec = NANOS_10_X_1000,
	};
	/* This argument is not in use */
	(void)event;
	os_mutex_lock(&cc_client->ctx.lock);
	// In case recv thread is already reading, waiting for completion
	// Need to make sure last meesage was read before we override the buffer
	while (cc_client->ctx.recv_flag) {
		nanosleep(&ts, &ts);
	}
	DOCA_LOG_INFO("[fd=%d] Message received: '%d", cc_client->ctx.fd, (int)msg_len);
	memcpy(cc_client->ctx.recv_buffer, recv_buffer, msg_len);
	cc_client->ctx.buf_size = (int)msg_len;
	cc_client->ctx.recv_flag = true;
	// Siganl to recv thread for copy done- recv thread can continue
	os_cond_signal(&cc_client->ctx.cond);
	os_mutex_unlock(&cc_client->ctx.lock);
}


/**
 * Callback for producer send task successfull completion
 *
 * @task [in]: Send task object
 * @task_user_data [in]: User data for task
 * @ctx_user_data [in]: User data for context
 */
static void
cc_producer_send_task_completion_callback(struct doca_cc_producer_send_task *task, union doca_data task_user_data,
					union doca_data ctx_user_data)
{
	(void)task_user_data;
	(void)ctx_user_data;
	struct doca_buf *buf;

	// DOCA_LOG_INFO("Producer task sent successfully");
	buf = doca_cc_producer_send_task_get_buf(task);
	(void)doca_buf_dec_refcount(buf, NULL);
	doca_task_free(doca_cc_producer_send_task_as_task(task));
}

/**
 * Callback for producer send task completion with error
 *
 * @task [in]: Send task object
 * @task_user_data [in]: User data for task
 * @ctx_user_data [in]: User data for context
 */
static void
cc_producer_send_task_completion_err_callback(struct doca_cc_producer_send_task *task, union doca_data task_user_data,
					   union doca_data user_data)
{
	struct doca_buf *buf;
	doca_error_t result;

	(void)task_user_data;

	struct cc_ctx_client *ctx_client = (struct cc_ctx_client *)user_data.ptr;
	result = doca_task_get_status(doca_cc_producer_send_task_as_task(task));
	DOCA_LOG_ERR("Producer message failed to send with error = %s",
			doca_error_get_name(result));

	buf = doca_cc_producer_send_task_get_buf(task);
	(void)doca_buf_dec_refcount(buf, NULL);
	doca_task_free(doca_cc_producer_send_task_as_task(task));
	(void)doca_ctx_stop(doca_cc_producer_as_ctx(ctx_client->ctx.ctx_fifo.producer));
}

static doca_error_t
cc_init_producer(struct cc_ctx *ctx)
{
	doca_error_t result;
	doca_data user_data;
	struct doca_ctx *doca_ctx;

	result = doca_cc_producer_create(ctx->connection, &(ctx->ctx_fifo.producer));
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create producer with error = %s", doca_error_get_name(result));
		return result;
	}
	doca_ctx = doca_cc_producer_as_ctx(ctx->ctx_fifo.producer);
	result = doca_pe_connect_ctx(ctx->ctx_fifo.pe, doca_ctx);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed adding pe context to producer with error = %s", doca_error_get_name(result));
	}
	result = doca_cc_producer_send_task_set_conf(ctx->ctx_fifo.producer, cc_producer_send_task_completion_callback,
						     cc_producer_send_task_completion_err_callback, CC_DATA_PATH_LOG_TASK_NUM);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed setting producer send task cbs with error = %s", doca_error_get_name(result));
	}

	user_data.ptr = (void*) ctx;
	result = doca_ctx_set_user_data(doca_ctx, user_data);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed setting producer user data with error = %s", doca_error_get_name(result));
		return result;
	}
	result = doca_ctx_start(doca_ctx);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to start producer context with error = %s", doca_error_get_name(result));
	}
	return DOCA_SUCCESS;
}

/**
 * Init message on client
 *
 * @cc_ctx_client [in]: cc_ctx_client struct
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
cc_init_client_send_message(struct cc_ctx_client *cc_client)
{
	doca_error_t result;
	union doca_data user_data;

	result = doca_cc_client_get_connection(cc_client->client, &(cc_client->ctx.connection));
	if (result != DOCA_SUCCESS) {
	    DOCA_LOG_ERR("[fd=%d] Failed to get connection from client with error = %s", cc_client->ctx.fd, doca_error_get_name(result));
	    return result;
	}
	DOCA_LOG_INFO("[fd=%d] doca_cc_client_get_connection succeeded", cc_client->ctx.fd);

	user_data.ptr = (void *)cc_client;
	result = doca_cc_connection_set_user_data(cc_client->ctx.connection, user_data);
	if (result != DOCA_SUCCESS) {
	    DOCA_LOG_ERR("[fd=%d] Failed to set user_data for connection with error = %s", cc_client->ctx.fd, doca_error_get_name(result));
	    return result;
	}

	if (cc_client->ctx.fast_path) {
		/* Init a cc producer */
		result = cc_init_producer(&cc_client->ctx);
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("[fd=%d] Failed to init a producer with error = %s", cc_client->ctx.fd, doca_error_get_name(result));
		}
		result = cc_init_consumer(&cc_client->ctx);
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("[fd=%d] Failed to init a consumer with error = %s", cc_client->ctx.fd, doca_error_get_name(result));
		}
	}
	cc_client->ctx.state = CC_CONNECTED;
	DOCA_LOG_INFO("[fd=%d] init_client_send_message succeeded", cc_client->ctx.fd);
	return DOCA_SUCCESS;
}

/**
 * Callback triggered whenever CC client context state changes
 *
 * @user_data [in]: User data associated with the CC client context. Will hold struct cc_ctrl_path_objects *
 * @ctx [in]: The CC client context that had a state change
 * @prev_state [in]: Previous context state
 * @next_state [in]: Next context state (context is already in this state when the callback is called)
 */
static void
cc_client_state_changed_callback(const union doca_data user_data, struct doca_ctx *ctx, enum doca_ctx_states prev_state,
				enum doca_ctx_states next_state)
{
	(void)ctx;
	(void)prev_state;
	doca_error_t result;
	struct cc_ctx_client *cc_client = (struct cc_ctx_client *)user_data.ptr;

	switch (next_state) {
	case DOCA_CTX_STATE_IDLE:
		DOCA_LOG_INFO("[fd=%d] CC client context has been stopped.", cc_client->ctx.fd);
		break;
	case DOCA_CTX_STATE_STARTING:
		/**
		 * The context is in starting state, need to progress until connection with server is established.
		 */
		DOCA_LOG_INFO("[fd=%d] CC client context entered into starting state. Waiting for connection establishment", cc_client->ctx.fd);
		break;
	case DOCA_CTX_STATE_RUNNING:
		DOCA_LOG_INFO("[fd=%d] CC client context is running. initialize message", cc_client->ctx.fd);
		result = cc_init_client_send_message(cc_client);
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("[fd=%d] Failed to submit send task with error = %s", cc_client->ctx.fd, doca_error_get_name(result));
			(void)doca_ctx_stop(doca_cc_client_as_ctx(cc_client->client));
		}
		break;
	case DOCA_CTX_STATE_STOPPING:
		/**
		 * The context is in stopping, this can happen when fatal error encountered or when stopping context.
		 * doca_pe_progress() will cause all tasks to be flushed, and finally transition state to idle
		 */
		DOCA_LOG_INFO("[fd=%d] CC client context entered into stopping state. Waiting for connection termination", cc_client->ctx.fd);
		break;
	default:
		break;
	}
}
/**
 * Callback for new consumer arrival event
 *
 * @event [in]: New remote consumer event object
 * @cc_connection [in]: The connection related to the consumer
 * @id [in]: The ID of the new remote consumer
 */
static void
cc_client_new_consumer_callback(struct doca_cc_event_consumer *event, struct doca_cc_connection *cc_connection, uint32_t id)
{
	union doca_data user_data;
	struct doca_cc_client *cc_client;
	doca_error_t result;

	/* This argument is not in use */
	(void)event;

	cc_client = doca_cc_client_get_client_ctx(cc_connection);

	result = doca_ctx_get_user_data(doca_cc_client_as_ctx(cc_client), &user_data);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to get user data from ctx with error = %s", doca_error_get_name(result));
		return;
	}

	struct cc_ctx_client *ctx_client = (struct cc_ctx_client *)(user_data.ptr);
	ctx_client->ctx.ctx_fifo.remote_consumer_id = id;

	ctx_client->ctx.ctx_fifo.fifo_connection_state = CC_FIFO_CONNECTED;
	DOCA_LOG_INFO("[fd=%d] Got a new remote consumer with ID = [%d]",ctx_client->ctx.fd, id);
}

/**
 * Callback for expired consumer arrival event
 *
 * @event [in]: Expired remote consumer event object
 * @cc_connection [in]: The connection related to the consumer
 * @id [in]: The ID of the expired remote consumer
 */
static void
cc_client_expired_consumer_callback(struct doca_cc_event_consumer *event, struct doca_cc_connection *cc_connection, uint32_t id)
{
	/* These arguments are not in use */
	(void)event;
	(void)cc_connection;
	(void)id;
}

static doca_error_t
cc_doca_client_set_params(struct cc_ctx_client *cc_client)
{
	struct doca_ctx *ctx;
	doca_error_t result;
	union doca_data user_data;

	ctx = doca_cc_client_as_ctx(cc_client->client);
	result = doca_ctx_set_state_changed_cb(ctx, cc_client_state_changed_callback);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed setting state change callback with error = %s", doca_error_get_name(result));
	}

	result = doca_cc_client_send_task_set_conf(cc_client->client, cc_client_send_task_completion_callback,
							cc_client_send_task_completion_err_callback, CC_SEND_TASK_NUM);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed setting send task cbs with error = %s", doca_error_get_name(result));
	}

	if (!cc_client->ctx.ctx_fifo.underload_mode) { // ping pong or throughput test
		result = doca_cc_client_event_msg_recv_register(cc_client->client, cc_client_message_recv_callback);
	} else { // underload test
		result = doca_cc_client_event_msg_recv_register(cc_client->client, cc_client_message_UL_recv_callback);
	}

	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed adding message recv event cb with error = %s", doca_error_get_name(result));
	}

	/* Set client properties */
	result = doca_cc_client_set_max_msg_size(cc_client->client, MSG_SIZE);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set msg size property with error = %s", doca_error_get_name(result));
	}

	result = doca_cc_client_set_recv_queue_size(cc_client->client, CC_REC_QUEUE_SIZE);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set msg size property with error = %s", doca_error_get_name(result));
	}

	if (cc_client->ctx.fast_path) { // Fast path option
		result = doca_cc_client_event_consumer_register(cc_client->client, cc_client_new_consumer_callback,
				cc_client_expired_consumer_callback);
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Failed adding consumer event cb with error = %s", doca_error_get_name(result));
		}
		struct cc_local_mem_bufs *local_consumer_mem = &(cc_client->ctx.ctx_fifo.consumer_mem);
		local_consumer_mem->mem = cc_client->ctx.recv_buffer;
		result = cc_init_local_mem_bufs(local_consumer_mem, &cc_client->ctx);
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Failed to consumer memory with error = %s", doca_error_get_name(result));
			return result;
		}
		DOCA_LOG_DBG("Init consumer memory succeeded");
	}

	user_data.ptr = (void *)cc_client;
	result = doca_ctx_set_user_data(ctx, user_data);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set ctx user data with error = %s", doca_error_get_name(result));
	}

	result = doca_ctx_start(ctx);
	if (result != DOCA_ERROR_IN_PROGRESS) {
		DOCA_LOG_ERR("Failed to start client context with error = %s", doca_error_get_name(result));
	}
	DOCA_LOG_DBG("[fd=%d] client properties setters succeeded", cc_client->ctx.fd);
	return result;
}

#endif // DOCA_CC_HELPER_H