/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef __SYNX_PRIVATE_H__
#define __SYNX_PRIVATE_H__

#include <linux/bitmap.h>
#include <linux/cdev.h>
#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>
#include <linux/idr.h>
#include <linux/workqueue.h>

#define SYNX_OBJ_NAME_LEN           64
#define SYNX_MAX_OBJS               1024
#define SYNX_MAX_REF_COUNTS         2048
#define SYNX_PAYLOAD_WORDS          4
#define SYNX_NAME                   "synx"
#define SYNX_WORKQUEUE_NAME         "hiprio_synx_work_queue"
#define SYNX_MAX_NUM_BINDINGS       8
#define SYNX_DEVICE_NAME            "synx_device"

/**
 * struct synx_external_data - data passed over to external sync objects
 * to pass on callback
 *
 * @synx_obj    : synx obj id
 * @secure_key  : secure key for authentication
 */
struct synx_external_data {
	s32 synx_obj;
	u32 secure_key;
};

/**
 * struct synx_bind_desc - bind payload descriptor
 *
 * @external_desc : external bind information
 * @bind_data     : pointer to data passed over
 */
struct synx_bind_desc {
	struct synx_external_desc external_desc;
	struct synx_external_data *external_data;
};

/**
 * struct synx_callback_info - Single node of information about a kernel
 * callback registered on a sync object
 *
 * @callback_func    : Callback function, registered by client driver
 * @cb_data          : Callback data, registered by client driver
 * @status           : Status with which callback will be invoked in client
 * @synx_obj         : Sync id of the object for which callback is registered
 * @cb_dispatch_work : Work representing the call dispatch
 * @list             : List member used to append this node to a linked list
 */
struct synx_callback_info {
	synx_callback callback_func;
	void *cb_data;
	int status;
	s32 synx_obj;
	struct work_struct cb_dispatch_work;
	struct list_head list;
};

struct synx_client;

/**
 * struct synx_user_payload - Single node of information about a callback
 * registered from user space
 *
 * @synx_obj     : Global id
 * @status       : synx obj status or callback failure
 * @payload_data : Payload data, opaque to kernel
 */
struct synx_user_payload {
	s32 synx_obj;
	int status;
	u64 payload_data[SYNX_PAYLOAD_WORDS];
};

/**
 * struct synx_cb_data - Single node of information about a user space
 * payload registered from user space
 *
 * @client : Synx client
 * @data   : Payload data, opaque to kernel
 * @list   : List member used to append this node to user cb list
 */
struct synx_cb_data {
	struct synx_client *client;
	struct synx_user_payload data;
	struct list_head list;
};

/**
 * struct synx_table_row - Single row of information about a synx object, used
 * for internal book keeping in the synx driver
 *
 * @name              : Optional string representation of the synx object
 * @fence             : dma fence backing the synx object
 * @synx_obj          : Integer id representing this synx object
 * @index             : Index of the spin lock table associated with synx obj
 * @num_bound_synxs   : Number of external bound synx objects
 * @signaling_id      : ID of the external sync object invoking the callback
 * @secure_key        : Secure key generated for authentication
 * @bound_synxs       : Array of bound synx objects
 * @callback_list     : Linked list of kernel callbacks registered
 * @user_payload_list : Linked list of user space payloads registered
 */
struct synx_table_row {
	char name[SYNX_OBJ_NAME_LEN];
	struct dma_fence *fence;
	s32 synx_obj;
	s32 index;
	u32 num_bound_synxs;
	s32 signaling_id;
	u32 secure_key;
	struct synx_bind_desc bound_synxs[SYNX_MAX_NUM_BINDINGS];
	struct list_head callback_list;
	struct list_head user_payload_list;
};

/**
 * struct bind_operations - Function pointers that need to be defined
 *    to achieve bind functionality for external fence with synx obj
 *
 * @register_callback   : Function to register with external sync object
 * @deregister_callback : Function to deregister with external sync object
 * @enable_signaling    : Function to enable the signaling on the external
 *                        sync object (optional)
 * @signal              : Function to signal the external sync object
 */
struct bind_operations {
	int (*register_callback)(synx_callback cb_func,
		void *userdata, s32 sync_obj);
	int (*deregister_callback)(synx_callback cb_func,
		void *userdata, s32 sync_obj);
	int (*enable_signaling)(s32 sync_obj);
	int (*signal)(s32 sync_obj, u32 status);
};

/**
 * struct synx_device - Internal struct to book keep synx driver details
 *
 * @cdev          : Character device
 * @dev           : Device type
 * @class         : Device class
 * @synx_table    : Table of all synx objects
 * @row_spinlocks : Spinlock array, one for each row in the table
 * @table_lock    : Mutex used to lock the table
 * @open_cnt      : Count of file open calls made on the synx driver
 * @work_queue    : Work queue used for dispatching kernel callbacks
 * @bitmap        : Bitmap representation of all synx objects
 * synx_ids       : Global unique ids
 * dma_context    : dma context id
 * bind_vtbl      : Table with bind ops for supported external sync objects
 * client_list    : All the synx clients
 */
struct synx_device {
	struct cdev cdev;
	dev_t dev;
	struct class *class;
	struct synx_table_row synx_table[SYNX_MAX_OBJS];
	spinlock_t row_spinlocks[SYNX_MAX_OBJS];
	struct mutex table_lock;
	int open_cnt;
	struct workqueue_struct *work_queue;
	DECLARE_BITMAP(bitmap, SYNX_MAX_OBJS);
	struct idr synx_ids;
	u64 dma_context;
	struct bind_operations bind_vtbl[SYNX_MAX_BIND_TYPES];
	struct list_head client_list;
};

/**
 * struct synx_client - Internal struct to book keep each client
 * specific details
 *
 * @device      : Pointer to synx device structure
 * @pid         : Process id
 * @eventq_lock : Spinlock for the event queue
 * @wq          : Queue for the polling process
 * @eventq      : All the user callback payloads
 * @list        : List member used to append this node to client_list
 */
struct synx_client {
	struct synx_device *device;
	int pid;
	spinlock_t eventq_lock;
	wait_queue_head_t wq;
	struct list_head eventq;
	struct list_head list;
};

#endif /* __SYNX_PRIVATE_H__ */