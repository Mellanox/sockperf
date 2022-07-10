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
#include "os_abstract.h"

#define MSG_SIZE 4080
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

enum cc_client_state {
	CONNECTION_IN_PROGRESS,
	CC_CONNECTED
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
	os_cond_t cond;						/**< For underload mode only>*/
};

struct cc_ctx_server {
	struct cc_ctx ctx;						/**< Base common ctx >*/
	struct doca_dev_rep *rep_dev;			/**< Device representor >*/
	struct doca_cc_server *server;			/**< Server object >*/
};

struct cc_ctx_client {
	struct cc_ctx ctx;						/**< Base common ctx >*/
	struct doca_cc_client *client;			/**< Client object >*/
	enum cc_client_state state; 			/**< Holding state of client connection >*/
	bool underload_mode;					/**< For using different callback>*/
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
	
	//DOCA_LOG_INFO("Message received: '%d, pointer is %p", (int)msg_len, recv_buffer);

	memcpy(ctx_server->ctx.recv_buffer, recv_buffer, msg_len);
	ctx_server->ctx.buf_size = (int)msg_len;
	ctx_server->ctx.recv_flag = true;
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
	user_data.ptr = (void *)ctx_server;

	result = doca_ctx_set_user_data(ctx, user_data);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set ctx user data with error = %s", doca_error_get_name(result));
	}

	result = doca_ctx_start(ctx);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to start server context with error = %s", doca_error_get_name(result));
	}
	DOCA_LOG_DBG("[fd=%d] server properties setters succeeded", ctx_server->ctx.fd);
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

	DOCA_LOG_INFO("Task sent successfully");

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

	DOCA_LOG_INFO("[fd=%d] Message received: '%d", cc_client->ctx.fd, (int)msg_len);
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

	cc_client->state = CC_CONNECTED;
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

	if (!cc_client->underload_mode) { // ping pong or throughput test
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