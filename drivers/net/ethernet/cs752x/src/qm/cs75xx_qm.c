/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/delay.h>
#include <asm/system.h>
#include <asm/io.h>
#include <linux/init.h>
#include "cs75xx_qm.h"

int qm_acp_enabled=0;
EXPORT_SYMBOL(qm_acp_enabled);
int use_internal_buff=0;
EXPORT_SYMBOL(use_internal_buff);
int internal_buff_size=CS_QM_INT_BUFF_0;
EXPORT_SYMBOL(internal_buff_size);

static int cs_qm_wait_access_done(unsigned int reg)
{
	unsigned int retry = CS_QM_RETRY_TIMES;
	unsigned int access;

	while (retry--) {
		access = readl(reg);
		if ((access & CS_QM_ACCESS_BIT) == 0)
			break;
	}

	if ((access & CS_QM_ACCESS_BIT) == 1)
		return -1;

	return 0;
}


void cs_qm_init_cfg(void)
{
	u32 mem_sz;
	QM_CONFIG_0_t qm_cfg0, qm_cfg0_mask;
	QM_CONFIG_1_t qm_cfg1, qm_cfg1_mask;
	QM_INT_BUF_CONFIG_0_t qm_int_buf_cfg0, qm_int_buf_cfg0_mask;

	if(use_internal_buff == 1){
		/* Internal memory mode: Added by acarr on Feb-17-2011*/
		/* Internal mode default is on for GigE ports and recirculation engine*/
		printk("QM Driver: Internal Buffers ON\n");

		qm_int_buf_cfg0.wrd = 0;
		qm_int_buf_cfg0_mask.wrd = 0;

		qm_int_buf_cfg0.bf.use_internal = 1;
		qm_int_buf_cfg0_mask.bf.use_internal = 1;

#ifdef CONFIG_CS752X_HW_INTERNAL_BUFFERS_RECIRCULATION
		printk("QM Driver: Internal Buffers for recirculation ON\n");
		qm_int_buf_cfg0.bf.use_internal_for_recirc = 1;
		qm_int_buf_cfg0_mask.bf.use_internal_for_recirc = 1;
#endif

        switch (internal_buff_size) {
            case CS_QM_INT_BUFF_128:
    			printk("QM Driver: Half Size Internal Buffers ON - 128KB Packet\n");
    			printk(" - 128kB free\n");
    			qm_int_buf_cfg0.bf.last_buffer_addr = 0x1ff;
    			qm_int_buf_cfg0_mask.bf.last_buffer_addr = 0x3ff;

    			qm_int_buf_cfg0.bf.buffer_size = 1;
    			qm_int_buf_cfg0_mask.bf.buffer_size = 1;
                break;
            case CS_QM_INT_BUFF_192:
    			printk("QM Driver: Half Size Internal Buffers ON - 192KB Packet\n");
    			printk(" - 64kB free\n");
    			qm_int_buf_cfg0.bf.last_buffer_addr = 0x17f;
    			qm_int_buf_cfg0_mask.bf.last_buffer_addr = 0x3ff;

    			qm_int_buf_cfg0.bf.buffer_size = 1;
    			qm_int_buf_cfg0_mask.bf.buffer_size = 1;
                break;
            case CS_QM_INT_BUFF_256:
			    printk("QM Driver: Full Size Internal Buffers ON - 256KB Packet\n");
                break;
        }

		write_reg(qm_int_buf_cfg0.wrd, qm_int_buf_cfg0_mask.wrd,
			  QM_INT_BUF_CONFIG_0);

	}
	else{
		printk("Use external DDR for QM\n");
	}

	qm_cfg0.wrd = 0;
	qm_cfg0_mask.wrd = 0;
	/* SET GLOBAL PARAMS CONFIGURATION */
	/* 1: Enables cpu full access to all memory */
	qm_cfg0.bf.cpu_full_access = 0;
	qm_cfg0.bf.max_pkt_len = MAX_PKT_LEN;	/* 0 ~ 9200 */
	qm_cfg0.bf.mtu = (unsigned char)(MAXIMUM_MTU + 14); /* not used in G2 */
	qm_cfg0.bf.egress_flow_cntl_mode = QM_EGRESS_FC_MODE_DISABLED;
	qm_cfg0.bf.egress_flow_cntl_int_threshold =
	    QM_EGRESS_FC_THR_2BY4TH_FIFO;
	qm_cfg0.bf.voq_disable_active = 0;
	qm_cfg0.bf.que_age_test_enable = 0;
	qm_cfg0.bf.pkt_age_test_enable = 0;
	qm_cfg0.bf.init = 0;
	qm_cfg0_mask.bf.cpu_full_access = 1;
	qm_cfg0_mask.bf.max_pkt_len = 0x3FFF;
	qm_cfg0_mask.bf.mtu = 0xFF;
	qm_cfg0_mask.bf.egress_flow_cntl_mode = 0x3;
	qm_cfg0_mask.bf.egress_flow_cntl_int_threshold = 0x3;
	qm_cfg0_mask.bf.voq_disable_active = 1;
	qm_cfg0_mask.bf.que_age_test_enable = 1;
	qm_cfg0_mask.bf.pkt_age_test_enable = 1;
	qm_cfg0_mask.bf.init = 1;
	write_reg(qm_cfg0.wrd, qm_cfg0_mask.wrd, QM_CONFIG_0);

#define QM_MEM_CFG_128MB	0
#define QM_MEM_CFG_256MB	1
#define QM_MEM_CFG_512MB	2
#define QM_MEM_CFG_1GB		3

	qm_cfg1.wrd = 0;
	qm_cfg1_mask.wrd = 0;
	
	mem_sz = readl(GLOBAL_SOFTWARE2) >> 20;
        switch(mem_sz){
        case 128:
                qm_cfg1.bf.mem_config = QM_MEM_CFG_128MB;
                break;
        case 256:
                qm_cfg1.bf.mem_config = QM_MEM_CFG_256MB;
                break;
        case 512:
                qm_cfg1.bf.mem_config = QM_MEM_CFG_512MB;
                break;
        case 1024:
                qm_cfg1.bf.mem_config = QM_MEM_CFG_1GB;
                break;
        default:
                printk("Unknow DRAM size!! Assume 512MB\n");
                qm_cfg1.bf.mem_config = QM_MEM_CFG_512MB;
        }

	qm_cfg1.bf.cpu_banks = 8 - CONFIG_CS752X_NR_QMBANK;

	/*
	 * When paging enable, SDRAM bursts do not cross page boundaries,
	 * 1 = enable.
	 */
	qm_cfg1.bf.paging_size = 1;	/*Specifies SDRAM page, 1 = 4k bytes */

	qm_cfg1_mask.bf.mem_config = 0x3;
	qm_cfg1_mask.bf.cpu_banks = 0x7;

	qm_cfg1_mask.bf.paging_size = 1;
	write_reg(qm_cfg1.wrd, qm_cfg1_mask.wrd, QM_CONFIG_1);

	/* ACP for QM */
#if 0
	writel(0x0000001F, GLOBAL_ARM_CONFIG_E);
	 /* ACP remap register */
	mem_sz = readl(GLOBAL_SOFTWARE2) >> 20;
	if(use_internal_buff == 0)
		mem_sz = mem_sz - (mem_sz >> 3)*CONFIG_CS752X_NR_QMBANK;

	mem_sz <<= 22;
 	writel(mem_sz, QM_SPARE);
#else
	mem_sz = readl(GLOBAL_SOFTWARE2) >> 20;
	if ((use_internal_buff==1) && (qm_acp_enabled==1)){
		/* QM ACP can be enabled only if internal buffer is used! */
		writel(0x0000001F, GLOBAL_ARM_CONFIG_E);
		mem_sz = mem_sz - (mem_sz >> 3)*CONFIG_CS752X_NR_QMBANK;
		mem_sz <<= 22;
		writel(mem_sz, QM_SPARE);
	}
	else{
		writel(0, QM_SPARE);
	}
#endif
}

static void cs_qm_init_profile_mem(void)
{
	int i;
	cs_qm_profile_mem_t profile_mem;

	for (i = 0; i < 112; i++){
		profile_mem.wrd = 0;
		switch (i) {
		case 0 ... 23: //Gige Port priority 3
			profile_mem.bf.packet_port = 1;
			profile_mem.bf.ds_speed = 500;
			profile_mem.bf.priority = 3;
			break;
		case 24 ... 39: //Recirculation Ports priority 1
			profile_mem.bf.packet_port = 0;
			profile_mem.bf.ds_speed = 500;
			profile_mem.bf.priority = 1;
			break;

		case 40 ... 47: //Root Port priority 4
			profile_mem.bf.packet_port = 0;
			profile_mem.bf.ds_speed = 500;
			profile_mem.bf.priority = 4;
			break;
		default: //CPU Port priority 0
			profile_mem.bf.packet_port = 0;
			profile_mem.bf.ds_speed = 500;
			profile_mem.bf.priority = 0;
			break;
		}
		if (cs_qm_set_profile_mem(i, &profile_mem))
			printk("%s: Set profile %d fail!\n", __func__, i);
	}
}

void cs_qm_init(void)
{
	QM_CONFIG_0_t qm_cfg0, qm_cfg0_mask;

	unsigned char init_done = 0;

	qm_cfg0.wrd = 0;
	qm_cfg0_mask.wrd = 0;
	qm_cfg0.bf.init = 1;
	qm_cfg0_mask.bf.init = 1;
	write_reg(qm_cfg0.wrd, qm_cfg0_mask.wrd, QM_CONFIG_0);
	/*printk("cs_qm_init\n");*/
	/* Indicates completion of initialization, 1=initialization done */
	do {
		init_done = (readl(QM_STATUS_0) & 0x1);
		/*printk("init_done = %d\n",init_done);*/
		udelay(10);
	} while (init_done == 0);

	qm_cfg0.wrd = 0;
	qm_cfg0_mask.wrd = 0;
	qm_cfg0.bf.init = 0;
	qm_cfg0_mask.bf.init = 1;
	write_reg(qm_cfg0.wrd, qm_cfg0_mask.wrd, QM_CONFIG_0);
	cs_qm_init_profile_mem();
}

void cs_qm_init_cpu_path_cfg(void)
{
	QM_CPU_PATH_CONFIG_0_t cpu_stat0_cfg, cpu_stat0_cfg_mask;
	QM_CPU_PATH_CONFIG_1_t cpu_stat1_cfg, cpu_stat1_cfg_mask;
	int i;
	unsigned int voq_map;

	cpu_stat0_cfg.wrd = 0;
	cpu_stat0_cfg_mask.wrd = 0;
	cpu_stat0_cfg.bf.cpu_buffers = MAX_CPU_BUFFERS;
	/* use default value 9000 */
	cpu_stat0_cfg.bf.max_cpu_pkt_len = MAX_CPU_PKT_LEN;
	cpu_stat0_cfg_mask.bf.cpu_buffers = 0x1FFF;
	cpu_stat0_cfg_mask.bf.max_cpu_pkt_len = 0x3FFF;
	write_reg(cpu_stat0_cfg.wrd, cpu_stat0_cfg_mask.wrd,
		  QM_CPU_PATH_CONFIG_0);

	cpu_stat1_cfg.wrd = 0;
	cpu_stat1_cfg_mask.wrd = 0;
	cpu_stat1_cfg.bf.linux_2byte_align_enable = 1;
	/*0=packet length; 1=32 bytes */
	cpu_stat1_cfg.bf.linux_mode_sch_rpt_size = 0;
	cpu_stat1_cfg.bf.linux_mode = 1;	/* 1=Linux Mode */

	cpu_stat1_cfg_mask.bf.linux_2byte_align_enable = 1;
	cpu_stat1_cfg_mask.bf.linux_mode_sch_rpt_size = 1;
	cpu_stat1_cfg_mask.bf.linux_mode = 1;
	write_reg(cpu_stat1_cfg.wrd, cpu_stat1_cfg_mask.wrd,
		  QM_CPU_PATH_CONFIG_1);
	/*
	 * VOQ <=> QM Linux buffer list mapping:
	 * CPU VoQ (0~7)   <=> QM linux queue (0~7)
	 * CPU VoQ (8~ 15) <=> QM linux queue (0~7)
	 *
	 */

	/* temp write for debug purpose */
	for (i = 0; i < 8; i++) {
		//writel(0x00fac688, QM_CPU_PATH_VOQ_MAP_0 + (i * 4));
		voq_map = i | (i << 3) | (i << 6) | (i << 9) | (i << 12) |
		    (i << 15) | (i << 18) | (i << 21);
		writel(voq_map, QM_CPU_PATH_VOQ_MAP_0 + (i * 4));
	}
}

volatile u32 qm_voq_map_reg[8] = {
	QM_CPU_PATH_VOQ_MAP_0, QM_CPU_PATH_VOQ_MAP_1,
	QM_CPU_PATH_VOQ_MAP_2, QM_CPU_PATH_VOQ_MAP_3,
	QM_CPU_PATH_VOQ_MAP_4, QM_CPU_PATH_VOQ_MAP_5,
	QM_CPU_PATH_VOQ_MAP_6, QM_CPU_PATH_VOQ_MAP_7
};

volatile u32 qm_linuxq_sdram_reg[8] = {
	QM_CPU_PATH_LINUX0_SDRAM_ADDR, QM_CPU_PATH_LINUX1_SDRAM_ADDR,
	QM_CPU_PATH_LINUX2_SDRAM_ADDR, QM_CPU_PATH_LINUX3_SDRAM_ADDR,
	QM_CPU_PATH_LINUX4_SDRAM_ADDR, QM_CPU_PATH_LINUX5_SDRAM_ADDR,
	QM_CPU_PATH_LINUX6_SDRAM_ADDR, QM_CPU_PATH_LINUX7_SDRAM_ADDR
};

/* {REG, SHIFT_BITS} */
volatile u32 qm_cpu_free_buffers[8][2] = {
	{QM_CPU_PATH_FREE_BUFFERS_0, 0},
	{QM_CPU_PATH_FREE_BUFFERS_0, 16},
	{QM_CPU_PATH_FREE_BUFFERS_1, 0},
	{QM_CPU_PATH_FREE_BUFFERS_1, 16},
	{QM_CPU_PATH_FREE_BUFFERS_2, 0},
	{QM_CPU_PATH_FREE_BUFFERS_2, 16},
	{QM_CPU_PATH_FREE_BUFFERS_3, 0},
	{QM_CPU_PATH_FREE_BUFFERS_3, 16},
};

volatile u32 qm_cpu_path_buffers[4] = {
	QM_CPU_PATH_BUFFERS_0,
	QM_CPU_PATH_BUFFERS_1,
	QM_CPU_PATH_BUFFERS_2,
	QM_CPU_PATH_BUFFERS_3
};

int cs_qm_exit(void)
{
	return 0;
}

int cs_qm_get_status_0(u32 * status)
{
	*status = readl(QM_STATUS_0);
	return 0;
}

int cs_qm_get_status_1(u32 * status)
{
	*status = readl(QM_STATUS_1);
	return 0;
}

int cs_qm_get_flush_status(u32 * status)
{
	*status = readl(QM_FLUSH_STATUS);
	return 0;
}

int cs_qm_flush_voq(QM_FLUSH_VOQ_t * reg)
{
	writel(reg->wrd, QM_FLUSH_VOQ);
	return 0;
}

int cs_qm_get_ingress_status_path_p0(u32 * status)
{
	*status = readl(QM_INGRESS_STATUS_PRIMARY_PATH0);
	return 0;
}

int cs_qm_get_ingress_status_path_p1(u32 * status)
{
	*status = readl(QM_INGRESS_STATUS_PRIMARY_PATH1);
	return 0;
}

int cs_qm_get_ingress_status_cpu_path0(u32 * status)
{
	*status = readl(QM_INGRESS_STATUS_CPU_PATH0);
	return 0;
}

int cs_qm_get_ingress_status_cpu_path1(u32 * status)
{
	*status = readl(QM_INGRESS_STATUS_CPU_PATH1);
	return 0;
}

int cs_qm_get_egress_status0(u32 * status)
{
	*status = readl(QM_EGRESS_STATUS_0);
	return 0;
}

int cs_qm_get_egress_status1(u32 * status)
{
	*status = readl(QM_EGRESS_STATUS_1);
	return 0;
}

/* we may define different mapping scheme later */
#define QM_CPU_PATH_VOQ_MAP_VAL	0xFAC688

int cs_qm_set_gbl_params(cs_qm_dev_cfg_t * cfg)
{
	int i;
	writel(cfg->cfg_0.wrd, QM_CONFIG_0);
	writel(cfg->cfg_1.wrd, QM_CONFIG_1);

	writel(cfg->cpu_path_cfg_0.wrd, QM_CPU_PATH_CONFIG_0);
	writel(cfg->cpu_path_cfg_1.wrd, QM_CPU_PATH_CONFIG_1);

	/* VoQ => Linux free buffer list mapping
	 * VoQ(8n+i) => LinuxQueue(i)
	 * (111 110 101 100 011 010 001 000)b
	 * or (0xFAC688)
	 */
	for (i = 0; i < 8; i++) {
		writel(QM_CPU_PATH_VOQ_MAP_VAL, qm_voq_map_reg[i]);
	}
	return 0;
}

int cs_qm_get_gbl_params(cs_qm_dev_cfg_t * cfg)
{
	if (cfg == NULL)
		return CS_ERROR;
	cfg->cfg_0.wrd = readl(QM_CONFIG_0);
	cfg->cfg_1.wrd = readl(QM_CONFIG_1);
	cfg->cpu_path_cfg_0.wrd = readl(QM_CPU_PATH_CONFIG_0);
	cfg->cpu_path_cfg_1.wrd = readl(QM_CPU_PATH_CONFIG_1);

	return 0;
}

/*
 * The caller should check linux mode
 */
int cs_qm_enqueue_cpu_buffer_list(unsigned char linux_qid, u32 buf_addr)
{
	writel(buf_addr, qm_linuxq_sdram_reg[linux_qid & 0x07]);
	return 0;
}

int cs_qm_egress_fc_bucket_init(cs_qm_dev_cfg_t * cfg)
{
	/* FIXME!! implement?*/
	return 0;
}

/* get total free CPU buffer cnt */
int cs_qm_get_free_cpu_buffer_cnt(u32 * cnt)
{
	QM_CPU_PATH_STATUS_0_t status_0;
	status_0.wrd = readl(QM_CPU_PATH_STATUS_0);
	*cnt = status_0.bf.free_cpu_buffers;
	return 0;
}

int cs_qm_get_fifo_wr_cnt(u32 * fifo_wr_cnt)
{
	QM_CPU_PATH_STATUS_0_t status_0;
	status_0.wrd = readl(QM_CPU_PATH_STATUS_0);
	*fifo_wr_cnt = status_0.bf.sdram_addr_fifo_wr_cnt;
	return 0;
}

int cs_qm_get_cpu_free_buffer_cnt(u32 qid, u32 * cnt)
{
	u32 buf_cnt;
	buf_cnt = readl(qm_cpu_free_buffers[qid][0]);
	buf_cnt >>= qm_cpu_free_buffers[qid][1];
	*cnt = (buf_cnt & 0x1fff);
	return 0;
}

int cs_qm_get_cpu_buffer_cnt(u32 qid, u32 * cnt)
{
	int idx = qid / 2;
	int low = qid % 2;
	qm_cpu_path_buffers_t buffers;

	buffers.wrd = readl(qm_cpu_path_buffers[idx]);
	if (low == 0)
		*cnt = buffers.bf.low;
	else
		*cnt = buffers.bf.high;

	return 0;
}

int cs_qm_set_rdcom_lp_fb_th(QM_RDCOM_LP_FB_TH_t val)
{
	writel(val.wrd, QM_RDCOM_LP_FB_TH);
	return 0;
}

int cs_qm_get_rdcom_lp_s_sat(QM_RDCOM_LP_S_SAT_t * reg)
{
	reg->wrd = readl(QM_RDCOM_LP_S_SAT);
	return 0;
}

int cs_qm_get_rdcom_lp_0_sat(QM_RDCOM_LP_O_SAT_t * reg)
{
	reg->wrd = readl(QM_RDCOM_LP_O_SAT);
	return 0;
}

int cs_qm_get_rdcom_lp_ld_th(QM_RDCOM_LP_LD_TH_t * reg)
{
	reg->wrd = readl(QM_RDCOM_LP_LD_TH);
	return 0;
}

int cs_qm_set_rdcom_lp_config(QM_RDCOM_LP_CONFIG_t * reg)
{
	writel(reg->wrd, QM_RDCOM_LP_CONFIG);
	return 0;
}

int cs_qm_get_rdcom_lp_config(QM_RDCOM_LP_CONFIG_t * reg)
{
	reg->wrd = readl(QM_RDCOM_LP_CONFIG);
	return 0;
}

int cs_qm_get_rdcom_lp_so_bk(QM_RDCOM_LP_SO_BK_t * reg)
{
	reg->wrd = readl(QM_RDCOM_LP_SO_BK);
	return 0;
}

int cs_qm_get_rdcom_lp_obbk_tav(QM_RDCOM_LP_OBBK_TAV_t * reg)
{
	reg->wrd = readl(QM_RDCOM_LP_OBBK_TAV);
	return 0;
}

int cs_qm_get_buffer_list_mem_status(QM_BUFFER_LIST_MEM_STATUS_t * reg)
{
	reg->wrd = readl(QM_BUFFER_LIST_MEM_STATUS);
	return 0;
}

int cs_qm_get_cpu_buffer_list_mem_status(
		QM_CPU_BUFFER_LIST_MEM_STATUS_t *reg)
{
	reg->wrd = readl(QM_CPU_BUFFER_LIST_MEM_STATUS);
	return 0;
}

int cs_qm_get_profile_mem_status(QM_PROFILE_MEM_STATUS_t * reg)
{
	reg->wrd = readl(QM_PROFILE_MEM_STATUS);
	return 0;
}

int cs_qm_get_status_mem_status(QM_STATUS_MEM_STATUS_t * reg)
{
	reg->wrd = readl(QM_STATUS_MEM_STATUS);
	return 0;
}

int cs_qm_get_status_sdram_addr_mem_status(
		QM_STATUS_SDRAM_ADDR_MEM_STATUS_t * reg)
{
	reg->wrd = readl(QM_STATUS_SDRAM_ADDR_MEM_STATUS);
	return 0;
}

int cs_qm_get_pkt_age_old(QM_PKT_AGE_OLD_t * reg)
{
	reg->wrd = readl(QM_PKT_AGE_OLD);
	return 0;
}

int cs_qm_queue_age_old(QM_QUE_AGE_OLD_t * reg)
{
	reg->wrd = readl(QM_QUE_AGE_OLD);
	return 0;
}

int cs_qm_get_int0(QM_INTERRUPT_0_t * int0)
{
	int0->wrd = readl(QM_INTERRUPT_0);
	return 0;
}

int cs_qm_set_int_enable(u32 reg, u32 val, u32 mask)
{
	u32 reg_val = readl(reg);
	reg_val = ((reg_val & (~mask)) | (val & mask));
	writel(reg_val, reg);
	return 0;
}

int cs_qm_get_int1(QM_INTERRUPT_1_t * int1)
{
	int1->wrd = readl(QM_INTERRUPT_1);
	return 0;
}

int cs_qm_get_profile_mem(unsigned char addr, cs_qm_profile_mem_t *profile_mem)
{
	QM_CONFIG_0_t qm_cfg;
	QM_QUE_PROFILE_MEM_ACCESS_t qm_profile_access;

	qm_cfg.wrd = readl(QM_CONFIG_0);
	qm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(qm_cfg.wrd, QM_CONFIG_0);

	qm_profile_access.bf.access = CS_ENABLE;
	qm_profile_access.bf.rbw = CS_OP_READ;
	qm_profile_access.bf.addr = addr;

	writel(qm_profile_access.wrd, QM_QUE_PROFILE_MEM_ACCESS);
	if (cs_qm_wait_access_done(QM_QUE_PROFILE_MEM_ACCESS) == -1)
		return -1;

	profile_mem->wrd = readl(QM_QUE_PROFILE_MEM_DATA);

	qm_cfg.wrd = readl(QM_CONFIG_0);
	qm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(qm_cfg.wrd, QM_CONFIG_0);

	return 0;
}

int cs_qm_set_profile_mem(unsigned char addr, cs_qm_profile_mem_t *profile_mem)
{
	QM_CONFIG_0_t qm_cfg;
	QM_QUE_PROFILE_MEM_ACCESS_t qm_profile_access;

	qm_cfg.wrd = readl(QM_CONFIG_0);
	qm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(qm_cfg.wrd, QM_CONFIG_0);

	qm_profile_access.bf.access = CS_ENABLE;
	qm_profile_access.bf.rbw = CS_OP_WRITE;
	qm_profile_access.bf.addr = addr;

	writel(profile_mem->wrd, QM_QUE_PROFILE_MEM_DATA);
	writel(qm_profile_access.wrd, QM_QUE_PROFILE_MEM_ACCESS);
	if (cs_qm_wait_access_done(QM_QUE_PROFILE_MEM_ACCESS) == -1)
		return -1;

	qm_cfg.wrd = readl(QM_CONFIG_0);
	qm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(qm_cfg.wrd, QM_CONFIG_0);

	return 0;
}

static int __init qm_use_internal_buff(char *str)
{
	unsigned int val;

	get_option(&str, &val);
	switch (val) {
	case 0:
		use_internal_buff=0;
		internal_buff_size=CS_QM_INT_BUFF_0;
		break;
	case 128:
		use_internal_buff=1;
		internal_buff_size=CS_QM_INT_BUFF_128;
		break;
	case 192:
		use_internal_buff=1;
		internal_buff_size=CS_QM_INT_BUFF_192;
		break;
	 case 256:
		use_internal_buff=1;
		internal_buff_size=CS_QM_INT_BUFF_256;
		break;
	default:
		use_internal_buff=0;
		internal_buff_size=CS_QM_INT_BUFF_0;
		break;
	}

	return 1;
}
__setup("qm_int_buff=", qm_use_internal_buff);

static int __init qm_acp_enable_set(char *str)
{
	unsigned int val;

	get_option(&str, &val);
	switch (val) {
	case 1:
		qm_acp_enabled=1;
		break;
	case 0:
	default:
		qm_acp_enabled=0;
		break;
	}

	return 1;
}
__setup("qm_acp_enable=", qm_acp_enable_set);
