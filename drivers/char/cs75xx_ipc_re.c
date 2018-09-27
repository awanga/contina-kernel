#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/kd.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/mm.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/vt_kern.h>
#include <linux/selection.h>
#include <linux/tiocl.h>
#include <linux/kbd_kern.h>
#include <linux/consolemap.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/pm.h>
#include <linux/font.h>
#include <linux/bitops.h>
#include <linux/notifier.h>
#include <linux/device.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <mach/g2cpu_ipc.h>
#include <mach/hardware.h>

#include <mach/cs75xx_ipc_wfo.h>

MODULE_LICENSE("GPL");

#define IPC_CPU_NUMBER 3
#define IPC_LIST_SIZE 6144
#define IPC_ITEM_SIZE 128
#define IPC_DEFAULT_TIMEOUT (HZ)

#ifndef  IPC_CPU_NUMBER
#error "G2 IPC CPU number is not defined"
#endif

#ifndef  IPC_LIST_SIZE
#error "G2 IPC list size is not defined"
#endif

#ifndef  IPC_ITEM_SIZE
#error "G2 IPC item size in list is not defined"
#endif

#define G2_IPC_USED_MSG	0x00
#define	G2_IPC_ASYN_MSG	0x01
#define G2_IPC_SYNC_MSG	0x02
#define G2_IPC_ACK_MSG	0x03

#define G2_IRQ  IRQ_REGBUS_SOFT0

#if IPC_LIST_SIZE > 665536
#error "Modify data type in fifl_info, msg_header, and others related"
#endif

struct fifo_info {
	cs_uint16 first;
	cs_uint16 last;
} __attribute__ ((__packed__));

struct list_info {
	struct fifo_info low_fifo;	// low priority 
	struct fifo_info high_fifo;	// high priority 

	cs_uint16 free_offset;	// free list
	cs_uint16 reserverd_for_alignment;
} __attribute__ ((__packed__));

struct msg_header {
	struct ipc_addr src_addr;
	struct ipc_addr dst_addr;

	cs_uint16 priority:1;
	cs_uint16 ipc_flag:2;
	cs_uint16 msg_no:13;
	cs_uint16 payload_size;
	cs_uint16 trans_id;
	cs_uint16 next_offset;
} __attribute__ ((__packed__));

struct ipc_context {
	struct list_head list;

	struct ipc_addr addr;
	cs_uint16 trans_id;
	void *private_data;

	struct g2_ipc_msg *msg_procs;
	cs_uint16 msg_number;
	cs_uint16 invoke_number;

	cs_uint16 wait_trans_id;
	spinlock_t lock;
	struct completion complete;

	struct msg_header *ack_item;
};

struct IPC_Module {
	struct ipc_addr addr;
	ulong shm_addr;
	ulong list_addr;
	ulong mbox_addr;
	ulong lock_addr;
	struct work_struct work;
	struct fifo_info wait_queue;
	spinlock_t lock;
	struct list_head client_list;
};

static struct ipc_context *findClient(struct IPC_Module *context,
				      cs_uint8 client_id)
{
	struct list_head *ptr;
	struct ipc_context *client;

	list_for_each(ptr, &context->client_list) {
		client = list_entry(ptr, struct ipc_context, list);
		if (client->addr.client_id == client_id) {
			return client;
		}
	}

	return NULL;
}

static struct IPC_Module *module_context;

static void initial_list_info(cs_uint8 cpu_id)
{
	cs_uint16 offset;
	unsigned int i, max_item_no;
	struct list_info *list;
	struct msg_header *ptr;
	unsigned long flags;

	max_item_no = (IPC_LIST_SIZE - sizeof(struct list_info)) /
	    IPC_ITEM_SIZE;

	offset = sizeof(struct list_info);

	for (i = 0; i < max_item_no; ++i) {
		ptr = (struct msg_header *)(module_context->shm_addr + IPC_LIST_SIZE * cpu_id + offset);
		offset += IPC_ITEM_SIZE;
		ptr->next_offset = offset;
	}
	ptr->next_offset = 0;

	list = (struct list_info *)module_context->shm_addr + IPC_LIST_SIZE * cpu_id;

	spin_lock_irqsave(&module_context->lock, flags);

	list->free_offset = sizeof(struct list_info);
	list->low_fifo.first = list->low_fifo.last =
	    list->high_fifo.first = list->high_fifo.last = 0;

	spin_unlock_irqrestore(&module_context->lock, flags);

	return;
}

void cs_ipc_print_status(cs_uint8 cpu_id)
{
	cs_uint16 offset;
	unsigned int i, max_item_no;
	struct list_info *list;
	struct msg_header *ptr;
	unsigned long flags;

	/* Print list info */
	list = (struct list_info *)(module_context->shm_addr + IPC_LIST_SIZE * cpu_id);
	spin_lock_irqsave(&module_context->lock, flags);

	printk("========= Dump IPC status of cpu %d =========\n", cpu_id);
	printk("free offset = %d\n", list->free_offset);
	printk("hi fifo from %d to %d\n", list->high_fifo.first, list->high_fifo.last);
	printk("low fifo from %d to %d\n", list->low_fifo.first, list->low_fifo.last);

	spin_unlock_irqrestore(&module_context->lock, flags);

	/* Print next offset */
	max_item_no = (IPC_LIST_SIZE - sizeof(struct list_info)) /
	    IPC_ITEM_SIZE;

	offset = sizeof(struct list_info);

	printk("---- print each msg ----\n");
	for (i = 0; i < max_item_no; ++i) {
		ptr = (struct msg_header *)(module_context->shm_addr + IPC_LIST_SIZE * cpu_id + offset);
		printk("%02d: offset = %d, next offset = %d\n", i, offset, ptr->next_offset);
		offset += IPC_ITEM_SIZE;
	}

	return;
}

void cs_ipc_reset_list_info(cs_uint8 cpu_id)
{
	initial_list_info(cpu_id);

	return;
}

//======================================================================
#ifdef G2_IPC_DEBUG
#define G2_DEBUG( fmt, args...) printk( KERN_DEBUG "G2_IPC:"fmt"\n", ##args)
#else
#define G2_DEBUG( fmt, args...)
#endif

int wfo_register_ipc_callback_function(u32 msg_no,u32 function)
{
	u32 i = 0,len; 
	int free_index = -1,get_it = -1;
	struct ipc_context *client;
	
	
	printk("Hook ipc callback function %x %x \r\n",msg_no,len);
	
	client = findClient(module_context, CS_WFO_IPC_PE0_CLNT_ID);
	if (NULL == client) {
		printk("No this client:id[%d]", CS_WFO_IPC_PE0_CLNT_ID);
		goto done;
	}	
	
	len = client->invoke_number+client->msg_number;

	for(i=0;i<len;i++){
		if(client->msg_procs[i].msg_no==msg_no){
			client->msg_procs[i].proc = function;
			get_it = 0;
		}
		if(client->msg_procs[i].msg_no==0)
			free_index = i;
	}
	
	if(get_it && (free_index>0)){
		client->msg_procs[free_index].msg_no = msg_no;
		client->msg_procs[free_index].proc = function;
	
	}	
done:	
	return free_index;
}EXPORT_SYMBOL(wfo_register_ipc_callback_function);

int g2_ipc_register(cs_uint8 client_id, const struct g2_ipc_msg *msg_procs,
		    cs_uint16 msg_count, cs_uint16 invoke_count,
		    void *private_data, struct ipc_context **context)
{
	struct ipc_context *client;
	cs_uint16 total_procs_no;

	total_procs_no = msg_count + invoke_count;
	if (0 == client_id || (0 != total_procs_no && NULL == msg_procs) ||
	    (0 == total_procs_no && NULL != msg_procs)) {
		G2_DEBUG("register parameters error:client[%d]", client_id);
		return G2_IPC_EINVAL;
	}

	client = findClient(module_context, client_id);
	if (NULL != client) {
		G2_DEBUG("has the client:id[%d]", client_id);
		return G2_IPC_EEXIST;
	}

	client = kmalloc(sizeof(struct ipc_context), GFP_KERNEL);
	if (NULL == client) {
		G2_DEBUG("malloc failed:client[%d]", client_id);
		return G2_IPC_ENOMEM;
	}

	if (0 == total_procs_no) {
		client->msg_procs = NULL;
	} else {
		client->msg_procs =
		    (struct g2_ipc_msg *)kmalloc(sizeof(struct g2_ipc_msg) *
						 total_procs_no, GFP_KERNEL);

		if (NULL == client->msg_procs) {
			kfree(client->msg_procs);
			kfree(client);
			G2_DEBUG("malloc failed:client[%d]", client_id);
			return G2_IPC_ENOMEM;
		}

		memcpy(client->msg_procs, msg_procs,
		       sizeof(struct g2_ipc_msg) * total_procs_no);
	}

	client->msg_number = msg_count;
	client->invoke_number = invoke_count;
	client->addr.cpu_id = module_context->addr.cpu_id;
	client->addr.client_id = client_id;
	client->trans_id = 0;
	client->private_data = private_data;
	client->wait_trans_id = 0;
	client->ack_item = NULL;

	spin_lock_init(&client->lock);
	init_completion(&client->complete);
	INIT_LIST_HEAD(&client->list);

	list_add(&client->list, &(module_context->client_list));
	*context = client;

	G2_DEBUG("register succesfull:client[%d]", client_id);
	return G2_IPC_OK;
}

int g2_ipc_deregister(struct ipc_context *context)
{
	struct list_head *ptr;
	struct ipc_context *client;
	cs_uint8 client_id;

	if (NULL == context) {
		G2_DEBUG("%s parameters invalid", __func__);
	}

	client_id = context->addr.client_id;

	list_for_each(ptr, &module_context->client_list) {
		client = list_entry(ptr, struct ipc_context, list);
		if (client == context) {
			list_del(ptr);
			kfree(client->msg_procs);
			kfree(client);

			G2_DEBUG("client deregister:client[%d]", client_id);
			return 0;
		}
	}

	G2_DEBUG("No such client:client[%d]", client_id);
	return 0;
}

static unsigned long get_list_item(cs_uint8 cpu_id)
{
	cs_uint16 offset;
	struct list_info *list;
	struct msg_header *tmp;
	unsigned long flags;
	unsigned long list_addr;

	offset = 0;
	list_addr = module_context->shm_addr + IPC_LIST_SIZE * cpu_id;
	list = (struct list_info *)list_addr;

	spin_lock_irqsave(&module_context->lock, flags);

	// Protect DS using MB0, to manipulate free_list 
	writel(1, module_context->lock_addr);
	while ((readl(module_context->lock_addr + 0x10) != 0x1)) {
		//printk("Did not get lock\n");
	}

	if (list->free_offset != 0) {
		offset = list->free_offset;
		tmp = (struct msg_header *)(list_addr + offset);
		list->free_offset = tmp->next_offset;
	}
	writel(0, module_context->lock_addr);

	spin_unlock_irqrestore(&module_context->lock, flags);

	return offset;
}

static void free_list_item(cs_uint8 cpu_id, struct msg_header *item)
{
	unsigned long offset;
	struct list_info *list;
	unsigned long flags;
	unsigned long list_addr;

	list_addr = module_context->shm_addr + cpu_id * IPC_LIST_SIZE;
	list = (struct list_info *)list_addr;
	offset = (unsigned long)item - list_addr;

#ifdef G2_IPC_DEBUG
	if (offset < sizeof(struct list_info) || IPC_LIST_SIZE < offset) {
		G2_DEBUG("Something is wrong. Check it");
		return;
	}
#endif

	spin_lock_irqsave(&module_context->lock, flags);

	// Protect DS using MB0, to manipulate free_list 
	writel(1, module_context->lock_addr);
	while ((readl(module_context->lock_addr + 0x10) != 0x1)) {
	}

	item->next_offset = list->free_offset;
	list->free_offset = offset;

	writel(0, module_context->lock_addr);

	spin_unlock_irqrestore(&module_context->lock, flags);
}

static int fill_header(struct ipc_context *context, cs_uint8 cpu_id,
		       cs_uint8 client_id, cs_uint8 ipc_flag, cs_uint8 priority,
		       cs_uint16 msg_no, cs_uint16 msg_size,
		       struct msg_header *header)
{
	if (NULL == context || IPC_CPU_NUMBER <= cpu_id ||
	    IPC_ITEM_SIZE < (msg_size + sizeof(struct msg_header))) {

		G2_DEBUG("Invalid Parameter");
		return G2_IPC_EINVAL;
	}

	header->src_addr = context->addr;
	header->dst_addr.cpu_id = cpu_id;
	header->dst_addr.client_id = client_id;
	header->ipc_flag = ipc_flag;
	header->priority = priority;
	header->msg_no = msg_no;
	header->payload_size = msg_size;

	context->trans_id += 1;
	header->trans_id = context->trans_id;

	return G2_IPC_OK;
}

static void write_dest_list(cs_uint8 cpu_id, cs_uint8 priority,
			    cs_uint16 offset)
{
	struct fifo_info *fifo;
	struct list_info *dest_list;
	struct msg_header *tmp;
	unsigned long flags;
	unsigned long list_addr;

#ifdef G2_IPC_DEBUG
	if (offset < sizeof(struct list_info) || IPC_LIST_SIZE < offset) {
		G2_DEBUG("Invalid [%u]. Something wrong. Check it.", offset);
		return;
	}
#endif
	list_addr = module_context->shm_addr + IPC_LIST_SIZE * cpu_id;
	dest_list = (struct list_info *)list_addr;
	tmp = (struct msg_header *)(list_addr + offset);
	tmp->next_offset = 0;

	if (G2_IPC_HPRIO == priority) {
		fifo = &dest_list->high_fifo;
	} else {
		fifo = &dest_list->low_fifo;
	}

	spin_lock_irqsave(&module_context->lock, flags);

	// Protect DS using MB0, to manipulate high_fifo & low_fifo 
	writel(1, module_context->lock_addr);
	while ((readl(module_context->lock_addr + 0x10) != 0x1)) {
	}

	if (0 == fifo->last) {
		fifo->first = fifo->last = offset;
	} else {
		tmp = (struct msg_header *)(module_context->shm_addr + 
						IPC_LIST_SIZE * cpu_id +
						fifo->last);
		fifo->last = tmp->next_offset = offset;
	}

	writel(0, module_context->lock_addr);

	//Send Event to Destination CPU 
	if (cpu_id == CPU_ARM) {
		writel(1, 0xf0070014);
	} else {
		G2_DEBUG("Writing to CPU (%d) mbox address 0x%x)...", cpu_id,
			 (module_context->mbox_addr + (cpu_id * 4)));
		/* Raise interrupt signal to target CPU. */
		if (readl(module_context->mbox_addr + (cpu_id * 4)) == 0)
			writel(1, (module_context->mbox_addr + (cpu_id * 4)));
		G2_DEBUG("Value %x\n",
			 readl(module_context->mbox_addr + (cpu_id * 4)));
	}
	spin_unlock_irqrestore(&module_context->lock, flags);
}

static int ipc_send_msg(struct ipc_context *context, cs_uint8 cpu_id,
			cs_uint8 client_id, cs_uint8 ipc_flag,
			cs_uint8 priority, cs_uint16 msg_no,
			const void *msg_data, cs_uint16 msg_size)
{
	unsigned rc;
	cs_uint16 offset;
	unsigned long send_timeout, flags;
	struct msg_header *header;

	send_timeout = jiffies + IPC_DEFAULT_TIMEOUT;

	do {
		offset = get_list_item(cpu_id);
		if (0 != offset) {
			break;
		}
		//schedule();
	} while (time_before(jiffies, send_timeout));

	if (time_after_eq(jiffies, send_timeout)) {
		return G2_IPC_EQFULL;
	}

	header = (struct msg_header *)(module_context->shm_addr +
				       cpu_id * IPC_LIST_SIZE + offset);

	rc = fill_header(context, cpu_id, client_id, ipc_flag, priority,
			 msg_no, msg_size, header);

	if (rc != G2_IPC_OK) {
		free_list_item(cpu_id, header);
		return rc;
	}

	if (G2_IPC_SYNC_MSG == ipc_flag) {
		spin_lock_irqsave(&context->lock, flags);
		context->wait_trans_id = header->trans_id;
		spin_unlock_irqrestore(&context->lock, flags);
	}

	memcpy(header + 1, msg_data, msg_size);

	write_dest_list(cpu_id, priority, offset);
	G2_DEBUG("Send Message to [%d:%d] OK", cpu_id, client_id);
	return G2_IPC_OK;
}

int g2_ipc_send(struct ipc_context *context, cs_uint8 cpu_id,
		cs_uint8 client_id, cs_uint8 priority, cs_uint16 msg_no,
		const void *msg_data, cs_uint16 msg_size)
{
	struct ipc_context *sender;

	if (NULL == context || IPC_CPU_NUMBER <= cpu_id ||
	    cpu_id == context->addr.cpu_id ||
	    (NULL == msg_data && 0 != msg_size) ||
	    (NULL != msg_data && 0 == msg_size)) {
		G2_DEBUG("Invalid Parameter");
		return G2_IPC_EINVAL;
	}

	sender = findClient(module_context, context->addr.client_id);
	if (NULL == sender) {
		G2_DEBUG("No Such Client");
		return G2_IPC_ENOCLIENT;
	}

	return ipc_send_msg(context, cpu_id, client_id, G2_IPC_ASYN_MSG,
			    priority, msg_no, msg_data, msg_size);
}

#ifdef _G2_IPC_SYNC__

int g2_ipc_invoke(struct ipc_context *context, cs_uint8 cpu_id,
		  cs_uint8 client_id, cs_uint8 priority, cs_uint16 msg_no,
		  const void *msg_data, cs_uint16 msg_size,
		  void *result_data, cs_uint16 * result_size)
{
	int rc;
	unsigned long flags;
	struct msg_header *ack_header;

	rc = ipc_send_msg(context, cpu_id, client_id, G2_IPC_SYNC_MSG,
			  priority, msg_no, msg_data, msg_size);

	if (G2_IPC_OK != rc) {
		return rc;
	}

	INIT_COMPLETION(context->complete);
	rc = wait_for_completion_interruptible_timeout(&context->complete,
						       IPC_DEFAULT_TIMEOUT);

	spin_lock_irqsave(&context->lock, flags);
	context->wait_trans_id = 0;
	spin_unlock_irqrestore(&context->lock, flags);

	if (0 > rc) {
		G2_DEBUG("Internal Error %d", rc);
		return G2_IPC_EINTERNAL;
	}

	if (0 == rc) {
		G2_DEBUG("Invoke timeout: client[%d:%d] call client[%d:%d]",
			 context->addr.cpu_id, context->addr.client_id,
			 cpu_id, client_id);

		return G2_IPC_ETIMEOUT;
	}

	rc = G2_IPC_EINTERNAL;
	if (G2_IPC_ACK_MSG == context->ack_item->ipc_flag) {
		ack_header = context->ack_item;

		if (0 < ack_header->payload_size) {
			if (ack_header->payload_size <= *result_size) {
				*result_size = ack_header->payload_size;
				memcpy(result_data, ack_header + 1,
				       *result_size);
				rc = G2_IPC_OK;
			} else {
				G2_DEBUG("Buffer is too small");
				rc = G2_IPC_ENOMEM;
			}
		} else {
			*result_size = 0;
			rc = G2_IPC_OK;
		}
	}

	free_list_item(context->addr.cpu_id, context->ack_item);
	return rc;
}

#endif

static unsigned long find_callback(struct g2_ipc_msg *messages,
				   cs_uint16 msg_number,
				   cs_uint16 search_target)
{
	unsigned i;
	for (i = 0; i < msg_number; ++i) {
		if (messages[i].msg_no == search_target) {
			break;
		}
	}

	if (i == search_target) {
		return 0;
	}

	return messages[i].proc;
}

static int do_asyn_message(struct ipc_context *client,
			   struct msg_header *header)
{
	unsigned long addr;
	ipc_msg_proc callback;

	addr = find_callback(client->msg_procs, client->msg_number,
			     header->msg_no);

	if (0 == addr) {
		G2_DEBUG("No intestesd message:"
			 "sender[%d:%d] msg_no[%d] receiver[%d:%d]",
			 header->src_addr.cpu_id,
			 header->src_addr.client_id, header->msg_no,
			 client->addr.cpu_id, client->addr.client_id);
	} else {
		callback = (ipc_msg_proc) addr;
		callback(header->src_addr, header->msg_no, header + 1,
			 header->payload_size, client);
	}
	free_list_item(client->addr.cpu_id, header);

	return 0;
}

#ifdef _G2_IPC_SYNC__

static int do_sync_message(struct ipc_context *client,
			   struct msg_header *header)
{
	ipc_invoke_proc callback;
	unsigned long addr, offset;
	struct msg_header *cb_buffer;

	addr = find_callback(client->msg_procs + client->msg_number,
			     client->invoke_number, header->msg_no);

	if (0 == addr) {
		G2_DEBUG("No intestesd message:"
			 "sender[%d:%d] msg_no[%d] receiver[%d:%d]",
			 header->src_addr.cpu_id,
			 header->src_addr.client_id, header->msg_no,
			 client->addr.cpu_id, client->addr.client_id);
		goto cleanup;
	}

	callback = (ipc_invoke_proc) addr;

	offset = get_list_item(header->src_addr.cpu_id);
	if (0 == offset) {
		goto cleanup;
	}

	cb_buffer = (struct msg_header *)(module_context->shm_addr +
					  header->src_addr.cpu_id *
					  IPC_LIST_SIZE + offset);

	cb_buffer->payload_size = IPC_ITEM_SIZE - sizeof(struct msg_header);

	callback(header->src_addr, header->msg_no, header + 1,
		 header->payload_size, cb_buffer + 1,
		 &cb_buffer->payload_size, client);

	fill_header(client, header->src_addr.cpu_id,
		    header->src_addr.client_id, G2_IPC_ACK_MSG,
		    header->priority, header->trans_id,
		    cb_buffer->payload_size, cb_buffer);

	write_dest_list(header->src_addr.cpu_id, header->priority, offset);

      cleanup:
	free_list_item(client->addr.cpu_id, header);
	return 1;
}

static int do_ack_message(struct ipc_context *client, struct msg_header *header)
{
	int rc;
	unsigned long offset, flags;

	if (NULL == client || NULL == header) {
		return 0;
	}

	spin_lock_irqsave(&client->lock, flags);
	rc = client->wait_trans_id != header->msg_no;
	spin_unlock_irqrestore(&client->lock, flags);

	if (rc) {
		G2_DEBUG("No intestesd message:"
			 "sender[%d:%d] msg_no[%d] receiver[%d:%d]",
			 header->src_addr.cpu_id,
			 header->src_addr.client_id, header->msg_no,
			 client->addr.cpu_id, client->addr.client_id);
		offset = (unsigned long)header - module_context->list_addr;
		free_list_item(client->addr.cpu_id, header);
		return 0;
	}

	spin_lock_irqsave(&client->lock, flags);
	client->wait_trans_id = 0;
	spin_unlock_irqrestore(&client->lock, flags);

	client->ack_item = header;
	complete(&client->complete);

	G2_DEBUG("Received ack message");
	return 1;
}

#endif

static void do_message(struct work_struct *work)
{
	unsigned long flags, offset;
	struct ipc_context *client;
	struct msg_header *item;

	struct list_info *list;

//printk("%s:%d:got here.\n", __func__, __LINE__);
	list = (struct list_info *)(module_context->shm_addr +
				    IPC_LIST_SIZE *
				    module_context->addr.cpu_id);

	do {
		item = NULL;

		// pop from wait list
		spin_lock_irqsave(&module_context->lock, flags);

		offset = module_context->wait_queue.first;

		if (0 != offset) {
			item = (struct msg_header *)
			    (module_context->list_addr + offset);
			module_context->wait_queue.first = item->next_offset;
			if (0 == module_context->wait_queue.first) {
				module_context->wait_queue.last = 0;
			}

			item->next_offset = 0;
		}

		spin_unlock_irqrestore(&module_context->lock, flags);

		if (NULL == item) {
			break;
		}

		client = findClient(module_context, item->dst_addr.client_id);
		if (NULL == client) {
			free_list_item(module_context->addr.cpu_id, item);
			break;
		}
		G2_DEBUG("Recev msg_type[%d]", item->ipc_flag);

		switch (item->ipc_flag) {
		case G2_IPC_ASYN_MSG:
			do_asyn_message(client, item);
			break;
#ifdef _G2_IPC_SYNC__
		case G2_IPC_SYNC_MSG:
			do_sync_message(client, item);
			break;
		case G2_IPC_ACK_MSG:
			do_ack_message(client, item);
			break;
#endif
		default:
			free_list_item(module_context->addr.cpu_id, item);
			break;
		};
	} while (1);
}

static irqreturn_t list_proc(int irq, void *device)
{
	struct list_info *list;

	struct fifo_info *fifo;
	unsigned long offset;
	struct msg_header *tmp;
	unsigned long flags;

	writel(0, PER_IRQ_SOFT0);
	G2_DEBUG("IPC IRQ");

	list = (struct list_info *)(module_context->shm_addr +
				    IPC_LIST_SIZE *
				    module_context->addr.cpu_id);

	offset = 0;
	do {
		fifo = NULL;
		spin_lock_irqsave(&module_context->lock, flags);

		if (0 != list->high_fifo.first) {
			fifo = &list->high_fifo;
		} else if (0 != list->low_fifo.first) {
			fifo = &list->low_fifo;
		} else {
			fifo = NULL;
		}

		if (NULL != fifo) {
			offset = fifo->first;

			// pop from read queue, 
			tmp =
			    (struct msg_header *)(module_context->list_addr +
						  offset);

			// Protect DS using MB0, to manipulate high_fifo & low_fifo 
			writel(1, module_context->lock_addr);
			while ((readl(module_context->lock_addr + 0x10) != 0x1)) {
			}
			fifo->first = tmp->next_offset;

			if (0 == tmp->next_offset) {
				fifo->last = 0;
			}
			writel(0, module_context->lock_addr);

			fifo = &module_context->wait_queue;
			// push into wait queue
			if (0 == fifo->last) {
				fifo->first = fifo->last = offset;
			} else {
				tmp = (struct msg_header *)
				    (module_context->shm_addr + fifo->last);
				fifo->last = tmp->next_offset = offset;
			}
		}

		spin_unlock_irqrestore(&module_context->lock, flags);

		schedule_work(&module_context->work);
	} while (NULL != fifo);

	return IRQ_HANDLED;
}

static short CPU_ID = CPU_ARM;

static int __init g2_ipc_init(void)
{
	int rc;
	module_context = kmalloc(sizeof(struct IPC_Module), GFP_KERNEL);
	if (NULL == module_context) {
		G2_DEBUG("Allocate memory failed");
		return G2_IPC_ENOMEM;
	}

	module_context->addr.cpu_id = CPU_ID;

#ifdef FIXME
#if CONFIG_G2_IPC2RCPU_SHMADDR == 0
	if (0 != CPU_ID) {
		module_context->shm_addr = SHM_ADDR;
	} else {
		module_context->shm_addr =
		    (unsigned long)kzalloc(IPC_CPU_NUMBER * IPC_LIST_SIZE,
					   GFP_KERNEL);
	}
#else
	module_context->shm_addr = ioremap(CONFIG_G2_IPC2RCPU_SHMADDR,
					   IPC_CPU_NUMBER * IPC_LIST_SIZE);
#endif
#else

	/* 2MB space is reserved for IPC for FPGA platform it is @ 0xE1E00000
	 * and for EB it @ 0x01E000000 and virtually mapped @ 0xFFDF0000
	 * FIXME - Looks like we can't use IOADDRESS marcro to get Virtual address
	 * from physical address.  Replace hardcoding after confirmation. */

#ifdef CONFIG_CORTINA_FPGA
	module_context->shm_addr = GOLDENGATE_IPC_BASE_VADDR;
#else
	module_context->shm_addr = GOLDENGATE_IPC_BASE_VADDR;
#endif
	printk("Debug:...Clearing memory....\n");
	memset(module_context->shm_addr, 0x0, (IPC_LIST_SIZE * IPC_CPU_NUMBER));
	module_context->mbox_addr =
		ioremap(RECIRC_TOP_RECPU_CPU_1_MAILBOX1_REQ, 12);
	module_context->lock_addr =
		ioremap(RECIRC_TOP_RECPU_CPU_1_MAILBOX0_REQ, 32);
#endif

	module_context->list_addr =
		module_context->shm_addr + CPU_ID * IPC_LIST_SIZE;

	spin_lock_init(&module_context->lock);
	INIT_WORK(&module_context->work, do_message);
	G2_DEBUG("cpu_id[%d] memory address %lu \n", CPU_ID,
		 module_context->shm_addr);
	G2_DEBUG("Remap address = %x\n", module_context->mbox_addr);

	module_context->addr.client_id = 0;
	module_context->wait_queue.first = module_context->wait_queue.last = 0;

	initial_list_info(module_context->addr.cpu_id);
	INIT_LIST_HEAD(&module_context->client_list);

	rc = request_irq(G2_IRQ, list_proc, 0, "g2_ipc", NULL);
	if (0 < rc) {
		printk(KERN_WARNING "request irq failed[%d]", rc);
		kfree(module_context);
		return rc;
	}

	return 0;
}

static void __exit g2_ipc_exit(void)
{
	if (0 == module_context->addr.cpu_id) {
#ifdef FIXME
#if CONFIG_G2_IPC2RCPU_SHMADDR == 0
		kfree((void *)module_context->shm_addr);
#else
		iounmap(module_context->shm_addr);
#endif
#else
		iounmap(module_context->mbox_addr);
		iounmap(module_context->lock_addr);
#endif
	}

	free_irq(G2_IRQ, NULL);
	kfree(module_context);
}

module_init(g2_ipc_init);
module_exit(g2_ipc_exit);
