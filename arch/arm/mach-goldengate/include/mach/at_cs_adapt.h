/****************************************************************************
           Software License for Customer Use of Cortina Software
                         Grant Terms and Conditions

IMPORTANT NOTICE - READ CAREFULLY: This Software License for Customer Use
of Cortina Software ("LICENSE"); is the agreement which governs use of
software of Cortina Systems, Inc. and its subsidiaries ("CORTINA");,
including computer software (source code and object code); and associated
printed materials ("SOFTWARE");.  The SOFTWARE is protected by copyright laws
and international copyright treaties, as well as other intellectual property
laws and treaties.  The SOFTWARE is not sold, and instead is only licensed
for use, strictly in accordance with this document.  Any hardware sold by
CORTINA is protected by various patents, and is sold but this LICENSE does
not cover that sale, since it may not necessarily be sold as a package with
the SOFTWARE.  This LICENSE sets forth the terms and conditions of the
SOFTWARE LICENSE only.  By downloading, installing, copying, or otherwise
using the SOFTWARE, you agree to be bound by the terms of this LICENSE.
If you do not agree to the terms of this LICENSE, then do not download the
SOFTWARE.

DEFINITIONS:  "DEVICE" means the Cortina Systems? Daytona SDK product.
"You" or "CUSTOMER" means the entity or individual that uses the SOFTWARE.
"SOFTWARE" means the Cortina Systems? SDK software.

GRANT OF LICENSE:  Subject to the restrictions below, CORTINA hereby grants
CUSTOMER a non-exclusive, non-assignable, non-transferable, royalty-free,
perpetual copyright license to (1); install and use the SOFTWARE for
reference only with the DEVICE; and (2); copy the SOFTWARE for your internal
use only for use with the DEVICE.

RESTRICTIONS:  The SOFTWARE must be used solely in conjunction with the
DEVICE and solely with Your own products that incorporate the DEVICE.  You
may not distribute the SOFTWARE to any third party.  You may not modify
the SOFTWARE or make derivatives of the SOFTWARE without assigning any and
all rights in such modifications and derivatives to CORTINA.  You shall not
through incorporation, modification or distribution of the SOFTWARE cause
it to become subject to any open source licenses.  You may not
reverse-assemble, reverse-compile, or otherwise reverse-engineer any
SOFTWARE provided in binary or machine readable form.  You may not
distribute the SOFTWARE to your customers without written permission
from CORTINA.

OWNERSHIP OF SOFTWARE AND COPYRIGHTS. All title and copyrights in and the
SOFTWARE and any accompanying printed materials, and copies of the SOFTWARE,
are owned by CORTINA. The SOFTWARE protected by the copyright laws of the
United States and other countries, and international treaty provisions.
You may not remove any copyright notices from the SOFTWARE.  Except as
otherwise expressly provided, CORTINA grants no express or implied right
under CORTINA patents, copyrights, trademarks, or other intellectual
property rights.

DISCLAIMER OF WARRANTIES. THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY
EXPRESS OR IMPLIED WARRANTY OF ANY KIND, INCLUDING ANY IMPLIED WARRANTIES
OF MERCHANTABILITY, NONINFRINGEMENT, OR FITNESS FOR A PARTICULAR PURPOSE,
TITLE, AND NON-INFRINGEMENT.  CORTINA does not warrant or assume
responsibility for the accuracy or completeness of any information, text,
graphics, links or other items contained within the SOFTWARE.  Without
limiting the foregoing, you are solely responsible for determining and
verifying that the SOFTWARE that you obtain and install is the appropriate
version for your purpose.

LIMITATION OF LIABILITY. IN NO EVENT SHALL CORTINA OR ITS SUPPLIERS BE
LIABLE FOR ANY DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION, LOST
PROFITS, BUSINESS INTERRUPTION, OR LOST INFORMATION); OR ANY LOSS ARISING OUT
OF THE USE OF OR INABILITY TO USE OF OR INABILITY TO USE THE SOFTWARE, EVEN
IF CORTINA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
TERMINATION OF THIS LICENSE. This LICENSE will automatically terminate if
You fail to comply with any of the terms and conditions hereof. Upon
termination, You will immediately cease use of the SOFTWARE and destroy all
copies of the SOFTWARE or return all copies of the SOFTWARE in your control
to CORTINA.  IF you commence or participate in any legal proceeding against
CORTINA, then CORTINA may, in its sole discretion, suspend or terminate all
license grants and any other rights provided under this LICENSE during the
pendency of such legal proceedings.
APPLICABLE LAWS. Claims arising under this LICENSE shall be governed by the
laws of the State of California, excluding its principles of conflict of
laws.  The United Nations Convention on Contracts for the International Sale
of Goods is specifically disclaimed.  You shall not export the SOFTWARE
without first obtaining any required export license or other approval from
the applicable governmental entity, if required.  This is the entire
agreement and understanding between You and CORTINA relating to this subject
matter.
GOVERNMENT RESTRICTED RIGHTS. The SOFTWARE is provided with "RESTRICTED
RIGHTS." Use, duplication, or disclosure by the Government is subject to
restrictions as set forth in FAR52.227-14 and DFAR252.227-7013 et seq. or
its successor. Use of the SOFTWARE by the Government constitutes
acknowledgment of CORTINA's proprietary rights therein. Contractor or
Manufacturer is CORTINA.

Copyright (c); 2010 by Cortina Systems Incorporated
****************************************************************************/
#ifndef AT_CS_ADAPT_H
#define AT_CS_ADAPT_H

#define AT_WFO_DBG                   printk("********%s:%d called from %pS\n", __func__, __LINE__, __builtin_return_address(0))




#ifdef CS_ATH_WFO
/*
 * TX descriptor fields, from 9380 datasheet
 */
struct tx_word_0 {
#ifdef BIG_ENDIAN
	uint32	ath_id          : 16;	// 31:16
	uint32	desc_tx_rx      : 1;	// 15
	uint32	desc_ctrl_stat  : 1;	// 14
	uint32	res             : 2;	// 13:12
	uint32	tx_qnum         : 4;	// 11:8
	uint32	desc_length     : 8;	// 7:0
#else
	uint32	desc_length     : 8;	// 7:0
	uint32	tx_qnum         : 4;	// 11:8
	uint32	res             : 2;	// 13:12
	uint32	desc_ctrl_stat  : 1;	// 14
	uint32	desc_tx_rx      : 1;	// 15
	uint32	ath_id          : 16;	// 31:16
#endif
} __attribute__((__packed__));

struct tx_word_1 {
	uint32	next_link       : 32;
};

struct tx_word_2 {
	uint32	buf_ptr0        : 32;
};

struct tx_word_3 {
#ifdef BIG_ENDIAN
	uint32	res2            : 4;	// 31:28
	uint32	buf_len0        : 12;	// 27:16
	uint32	res1            : 16;	// 15:0
#else
	uint32	res1            : 16;	// 15:0
	uint32	buf_len0        : 12;	// 27:16
	uint32	res2            : 4;	// 31:28
#endif
} __attribute__((__packed__));

struct tx_word_4 {
	uint32	buf_ptr1        : 32;
};

struct tx_word_5 {
#ifdef BIG_ENDIAN
	uint32	res2            : 4;	// 31:28
	uint32	buf_len1        : 12;	// 27:16
	uint32	res1            : 16;
#else
	uint32	res1            : 16;
	uint32	buf_len1        : 12;	// 27:16
	uint32	res2            : 4;	// 31:28
#endif
}__attribute__((__packed__));

struct tx_word_6 {
	uint32	buf_ptr2        : 32;
};

struct tx_word_7 {
#ifdef BIG_ENDIAN
	uint32	res2            : 4;	// 31:28
	uint32	buf_len2        : 12;	// 27:16
	uint32	res1            : 16;
#else
	uint32	res1            : 16;
	uint32	buf_len2        : 12;	// 27:16
	uint32	res2            : 4;	// 31:28
#endif
}__attribute__((__packed__));

struct tx_word_8 {
	uint32	buf_ptr3        : 32;
};

struct tx_word_9 {
#ifdef BIG_ENDIAN
	uint32	res2            : 4;	// 31:28
	uint32	buf_len3        : 12;	// 27:16
	uint32	res1            : 16;
#else
	uint32	res1            : 16;
	uint32	buf_len3        : 12;	// 27:16
	uint32	res2            : 4;	// 31:28
#endif
}__attribute__((__packed__));

struct tx_word_10 {
#ifdef BIG_ENDIAN
	uint32	tx_desc_id      : 16;	// 31:16
	uint32	ptr_chksum      : 16;	// 15:0
#else
	uint32	ptr_chksum      : 16;	// 15:0
	uint32	tx_desc_id      : 16;	// 31:16
#endif
}__attribute__((__packed__));

struct tx_word_11 {
#ifdef BIG_ENDIAN
	uint32	cts_enable      : 1;	// 31
	uint32	dest_idx_vld    : 1;	// 30
	uint32	int_req         : 1;	// 29
	uint32	beam_form       : 4;	// 28:25
	uint32	clear_dest_mask : 1;	// 24
	uint32	veol            : 1;	// 23, virtual end of list flag
	uint32	rts_enable      : 1;	// 22
	uint32	tpc_0           : 6;	// 21:16
	uint32	clear_entry     : 1;	// 15
	uint32	low_rx_chain    : 1;	// 14
	uint32	fast_ant_mode   : 1;	// 13
	uint32	vmf             : 1;	// 12, virtual more flag
	uint32	frame_length    : 12;	// 11:0
#else
	uint32	frame_length    : 12;	// 11:0
	uint32	vmf             : 1;	// 12, virtual more flag
	uint32	fast_ant_mode   : 1;	// 13
	uint32	low_rx_chain    : 1;	// 14
	uint32	clear_entry     : 1;	// 15
	uint32	tpc_0           : 6;	// 21:16
	uint32	rts_enable      : 1;	// 22
	uint32	veol            : 1;	// 23, virtual end of list flag
	uint32	clear_dest_mask : 1;	// 24
	uint32	beam_form       : 4;	// 28:25
	uint32	int_req         : 1;	// 29
	uint32	dest_idx_vld    : 1;	// 30
	uint32	cts_enable      : 1;	// 31
#endif
}__attribute__((__packed__));

struct tx_word_12 {
#ifdef BIG_ENDIAN
	uint32	more_rifs       : 1;	// 31
	uint32	is_agg          : 1;	// 30
	uint32	more_agg        : 1;	// 29
	uint32	ext_and_ctl     : 1;	// 28
	uint32	res2            : 1;	// 27
	uint32	corrupt_fcs     : 1;	// 26
	uint32	res             : 1;	// 25
	uint32	no_ack          : 1;	// 24
	uint32	frame_type      : 4;	// 23:20
	uint32	dest_index      : 7;	// 19:13
	uint32	more            : 1;	// 12
	uint32	pa              : 3;	// 11:9
	uint32	res1            : 9;	// 8:0
#else
	uint32	res1            : 9;	// 8:0
	uint32	pa              : 3;	// 11:9
	uint32	more            : 1;	// 12
	uint32	dest_index      : 7;	// 19:13
	uint32	frame_type      : 4;	// 23:20
	uint32	no_ack          : 1;	// 24
	uint32	res             : 1;	// 25
	uint32	corrupt_fcs     : 1;	// 26
	uint32	res2            : 1;	// 27
	uint32	ext_and_ctl     : 1;	// 28
	uint32	more_agg        : 1;	// 29
	uint32	is_agg          : 1;	// 30
	uint32	more_rifs       : 1;	// 31
#endif
}__attribute__((__packed__));

struct tx_word_13 {
#ifdef BIG_ENDIAN
	uint32	tx_tries3       : 4;	// 31:28
	uint32	tx_tries2       : 4;	// 27:24
	uint32	tx_tries1       : 4;	// 23:20
	uint32	tx_tries0       : 4;	// 19:16
	uint32	dur_update_en   : 1;	// 15
	uint32	burst_duration  : 15;	// 14:0
#else
	uint32	burst_duration  : 15;	// 14:0
	uint32	dur_update_en   : 1;	// 15
	uint32	tx_tries0       : 4;	// 19:16
	uint32	tx_tries1       : 4;	// 23:20
	uint32	tx_tries2       : 4;	// 27:24
	uint32	tx_tries3       : 4;	// 31:28
#endif
}__attribute__((__packed__));

struct tx_word_14 {
#ifdef BIG_ENDIAN
	uint32	tx_rate3        : 8;	// 31:24
	uint32	tx_rate2        : 8;	// 23:16
	uint32	tx_rate1        : 8;	// 15:8
	uint32	tx_rate0        : 8;	// 7:0
#else
	uint32	tx_rate0        : 8;	// 7:0
	uint32	tx_rate1        : 8;	// 15:8
	uint32	tx_rate2        : 8;	// 23:16
	uint32	tx_rate3        : 8;	// 31:24
#endif
}__attribute__((__packed__));

struct tx_word_15 {
#ifdef BIG_ENDIAN
	uint32	rts_cts_qual1   : 1;	// 31
	uint32	pkt_duration1   : 15;	// 30:16
	uint32	rts_cts_qual0   : 1;	// 15
	uint32	pkt_duration0	: 15;	// 14:0
#else
	uint32	pkt_duration0	: 15;	// 14:0
	uint32	rts_cts_qual0   : 1;	// 15
	uint32	pkt_duration1   : 15;	// 30:16
	uint32	rts_cts_qual1   : 1;	// 31
#endif
}__attribute__((__packed__));

struct tx_word_16 {
#ifdef BIG_ENDIAN
	uint32	rts_cts_qual3   : 1;	// 31
	uint32	pkt_duration3	: 15;	// 30:16
	uint32	rts_cts_qual2	: 1;	// 15
	uint32	pkt_duration2	: 15;	// 14:0
#else
	uint32	pkt_duration2	: 15;	// 14:0
	uint32	rts_cts_qual2	: 1;	// 15
	uint32	pkt_duration3	: 15;	// 30:16
	uint32	rts_cts_qual3   : 1;	// 31
#endif
}__attribute__((__packed__));

struct tx_word_17 {
#ifdef BIG_ENDIAN
	uint32	res2            : 1;	// 31
	uint32	calibrating     : 1;	// 30
	uint32	dc_ap_sta_sel	: 1;	// 29
	uint32	encrypt_type    : 3;	// 28:26
	uint32	pad_delim       : 8;	// 25:18
	uint32	res1            : 2;	// 17:16
	uint32	agg_length      : 16;	// 15:0
#else
	uint32	agg_length      : 16;	// 15:0
	uint32	res1            : 2;	// 17:16
	uint32	pad_delim       : 8;	// 25:18
	uint32	encrypt_type    : 3;	// 28:26
	uint32	dc_ap_sta_sel	: 1;	// 29
	uint32	calibrating     : 1;	// 30
	uint32	res2            : 1;	// 31
#endif
}__attribute__((__packed__));

struct tx_word_18 {
#ifdef BIG_ENDIAN
	uint32	stbc            : 4;	// 31:28
	uint32	rts_cts_rate    : 8;	// 27:20
	uint32	chain_sel_3     : 3;	// 19:17
	uint32	guard_int_3     : 1;	// 16
	uint32	ht_20_40_3      : 1;	// 15
	uint32	chain_sel_2     : 3;	// 14:12
	uint32	guard_int_2     : 1;	// 11
	uint32	ht_20_40_2      : 1;	// 10
	uint32	chain_sel_1     : 3;	// 9:7
	uint32	guard_int_1     : 1;	// 6
	uint32	ht_20_40_1      : 1;	// 5
	uint32	chain_sel_0     : 3;	// 4:2
	uint32	guard_int_0     : 1;	// 1
	uint32	ht_20_40_0      : 1;	// 0
#else
	uint32	ht_20_40_0      : 1;	// 0
	uint32	guard_int_0     : 1;	// 1
	uint32	chain_sel_0     : 3;	// 4:2
	uint32	ht_20_40_1      : 1;	// 5
	uint32	guard_int_1     : 1;	// 6
	uint32	chain_sel_1     : 3;	// 9:7
	uint32	ht_20_40_2      : 1;	// 10
	uint32	guard_int_2     : 1;	// 11
	uint32	chain_sel_2     : 3;	// 14:12
	uint32	ht_20_40_3      : 1;	// 15
	uint32	guard_int_3     : 1;	// 16
	uint32	chain_sel_3     : 3;	// 19:17
	uint32	rts_cts_rate    : 8;	// 27:20
	uint32	stbc            : 4;	// 31:28
#endif
}__attribute__((__packed__));

struct tx_word_19 {
#ifdef BIG_ENDIAN
	uint32	ness_0          : 2;	// 31:30
	uint32	not_sounding    : 1;	// 29
	uint32	rts_htc_trq     : 1;	// 28
	uint32	rts_htc_mrq     : 1;	// 27
	uint32	rts_htc_msi     : 3;	// 26:24
	uint32	antenna_0       : 24;	// 23:0
#else
	uint32	antenna_0       : 24;	// 23:0
	uint32	rts_htc_msi     : 3;	// 26:24
	uint32	rts_htc_mrq     : 1;	// 27
	uint32	rts_htc_trq     : 1;	// 28
	uint32	not_sounding    : 1;	// 29
	uint32	ness_0          : 2;	// 31:30
#endif
}__attribute__((__packed__));

struct tx_word_20 {
#ifdef BIG_ENDIAN
	uint32	ness_1          : 2;	// 31:30
	uint32	tpc_1           : 6;	// 29:24
	uint32	antenna_1       : 24;	// 23:0
#else
	uint32	antenna_1       : 24;	// 23:0
	uint32	tpc_1           : 6;	// 29:24
	uint32	ness_1          : 2;	// 31:30
#endif
}__attribute__((__packed__));

struct tx_word_21 {
#ifdef BIG_ENDIAN
	uint32	ness_2          : 2;	// 31:30
	uint32	tpc_2           : 6;	// 29:24
	uint32	antenna_2       : 24;	// 23:0
#else
	uint32	antenna_2       : 24;	// 23:0
	uint32	tpc_2           : 6;	// 29:24
	uint32	ness_2          : 2;	// 31:30
#endif
}__attribute__((__packed__));

struct tx_word_22 {
#ifdef BIG_ENDIAN
	uint32	ness_3          : 2;	// 31:30
	uint32	tpc_3           : 6;	// 29:24
	uint32	antenna_3       : 24;	// 23:0
#else
	uint32	antenna_3       : 24;	// 23:0
	uint32	tpc_3           : 6;	// 29:24
	uint32	ness_3          : 2;	// 31:30
#endif
}__attribute__((__packed__));

typedef struct tx_desc {
	struct tx_word_0	word0;
	struct tx_word_1	word1;
	struct tx_word_2	word2;
	struct tx_word_3	word3;
	struct tx_word_4	word4;
	struct tx_word_5	word5;
	struct tx_word_6	word6;
	struct tx_word_7	word7;
	struct tx_word_8	word8;
	struct tx_word_9	word9;
	struct tx_word_10	word10;
	struct tx_word_11	word11;
	struct tx_word_12	word12;
	struct tx_word_13	word13;
	struct tx_word_14	word14;
	struct tx_word_15	word15;
	struct tx_word_16	word16;
	struct tx_word_17	word17;
	struct tx_word_18	word18;
	struct tx_word_19	word19;
	struct tx_word_20	word20;
	struct tx_word_21	word21;
	struct tx_word_22	word22;
} tx_desc_s;


/* TX Status descriptors */
struct txs_word_0 {
#ifdef BIG_ENDIAN
	uint32	ath_id          : 16;	// 31:16
	uint32	desc_tx_rx      : 1;	// 15
	uint32	desc_ctrl_stat  : 1;	// 14
	uint32	res             : 2;	// 13:12
	uint32	tx_qnum         : 4;	// 11:8
	uint32	desc_length     : 8;	// 7:0
#else
	uint32	desc_length     : 8;	// 7:0
	uint32	tx_qnum         : 4;	// 11:8
	uint32	res             : 2;	// 13:12
	uint32	desc_ctrl_stat  : 1;	// 14
	uint32	desc_tx_rx      : 1;	// 15
	uint32	ath_id          : 16;	// 31:16
#endif
}__attribute__((__packed__));

struct txs_word_1 {
#ifdef BIG_ENDIAN
	uint32	tx_desc_id      : 16;	// 31:16
	uint32	res             : 16;	// 15:0
#else
	uint32	res             : 16;	// 15:0
	uint32	tx_desc_id      : 16;	// 31:16
#endif
}__attribute__((__packed__));

struct txs_word_2 {
#ifdef BIG_ENDIAN
	uint32	res2            : 1;	// 31
	uint32	ba_status       : 1;	// 30
	uint32	res1            : 6;	// 29:24
	uint32	ack_rssi_ant02  : 8;	// 23:16
	uint32	ack_rssi_ant01  : 8;	// 15:8
	uint32	ack_rssi_ant00  : 8;	// 7:0
#else
	uint32	ack_rssi_ant00  : 8;	// 7:0
	uint32	ack_rssi_ant01  : 8;	// 15:8
	uint32	ack_rssi_ant02  : 8;	// 23:16
	uint32	res1            : 6;	// 29:24
	uint32	ba_status       : 1;	// 30
	uint32	res2            : 1;	// 31
#endif
}__attribute__((__packed__));

struct txs_word_3 {
#ifdef BIG_ENDIAN
	uint32	res2            : 12;	// 31:20
	uint32	tx_timer_exp    : 1;	// 19
	uint32	desc_cfg_err    : 1;	// 18
	uint32	tx_underrun     : 1;	// 17
	uint32	tx_dlm_underrun : 1;	// 16
	uint32	virt_retry_cnt  : 4;	// 15:12
	uint32	data_fail_cnt   : 4;	// 11:8
	uint32	rts_fail_cnt    : 4;	// 7:4
	uint32	filtered        : 1;	// 3
	uint32	fifo_underrun   : 1;	// 2
	uint32	excessive_retry : 1;	// 1
	uint32	frame_xmit_ok   : 1;	// 0
#else
	uint32	frame_xmit_ok   : 1;	// 0
	uint32	excessive_retry : 1;	// 1
	uint32	fifo_underrun   : 1;	// 2
	uint32	filtered        : 1;	// 3
	uint32	rts_fail_cnt    : 4;	// 7:4
	uint32	data_fail_cnt   : 4;	// 11:8
	uint32	virt_retry_cnt  : 4;	// 15:12
	uint32	tx_dlm_underrun : 1;	// 16
	uint32	tx_underrun     : 1;	// 17
	uint32	desc_cfg_err    : 1;	// 18
	uint32	tx_timer_exp    : 1;	// 19
	uint32	res2            : 12;	// 31:20
#endif
}__attribute__((__packed__));

struct txs_word_4 {
	uint32	send_timestamp  : 32;
};

struct txs_word_5 {
	uint32	ba_bitmap_0_31  : 32;
};

struct txs_word_6 {
	uint32	ba_bitmap_32_63 : 32;
};

struct txs_word_7 {
#ifdef BIG_ENDIAN
	uint32	ack_rssi_combo  : 8;	// 31:24
	uint32	ack_rssi_ant12  : 8;	// 23:16
	uint32	ack_rssi_ant11  : 8;	// 15:8
	uint32	ack_rssi_ant10  : 8;	// 7:0
#else
	uint32	ack_rssi_ant10  : 8;	// 7:0
	uint32	ack_rssi_ant11  : 8;	// 15:8
	uint32	ack_rssi_ant12  : 8;	// 23:16
	uint32	ack_rssi_combo  : 8;	// 31:24
#endif
}__attribute__((__packed__));

struct txs_word_8 {
#ifdef BIG_ENDIAN
	uint32	tid             : 4;	// 31:28
	uint32	res2            : 2;	// 27:26
	uint32	pwr_mgmt        : 1;	// 25
	uint32	txbf_exp_miss   : 1;	// 24
	uint32	txbf_dest_miss  : 1;	// 23
	uint32	final_tx_idx    : 2;	// 22:21
	uint32	res1            : 1;	// 20
	uint32	txbf_stream_miss : 1;	// 19
	uint32	txbf_bw_mismatch : 1;	// 18
	uint32	txop_exceeded   : 1;	// 17
	uint32	res0            : 4;	// 16:13
	uint32	sequence        : 12;	// 12:1
	uint32	done            : 1;	// 0
#else
	uint32	done            : 1;	// 0
	uint32	sequence        : 12;	// 12:1
	uint32	res0            : 4;	// 16:13
	uint32	txop_exceeded   : 1;	// 17
	uint32	txbf_bw_mismatch : 1;	// 18
	uint32	txbf_stream_miss : 1;	// 19
	uint32	res1            : 1;	// 20
	uint32	final_tx_idx    : 2;	// 22:21
	uint32	txbf_dest_miss  : 1;	// 23
	uint32	txbf_exp_miss   : 1;	// 24
	uint32	pwr_mgmt        : 1;	// 25
	uint32	res2            : 2;	// 27:26
	uint32	tid             : 4;	// 31:28
#endif
}__attribute__((__packed__));

typedef struct txs_desc {
	struct txs_word_0	word0;
	struct txs_word_1	word1;
	struct txs_word_2	word2;
	struct txs_word_3	word3;
	struct txs_word_4	word4;
	struct txs_word_5	word5;
	struct txs_word_6	word6;
	struct txs_word_7	word7;
	struct txs_word_8	word8;
} txs_desc_s;

#if 0
/*
 * MAC entry lookup table, for moving TX functions to PE
 */
typedef struct ath_wfo_mac_entry_s {
	uint16	dest_idx;	/* txdesc_12, 19:13 */
	uint8	mac_addr[6];
	uint8	status;
	uint8	qnum;
	uint16	seq_no;
	uint32	flags;
	uint8	power;		/* tx_desc_11, 21:16*/
	uint8	frame_type;	/* txdesc_12, 23:20 */
	uint8	encrypt_type;	/* txdesc_17, 28:26 */
	uint8	pad_delim;	/* txdesc_17, 25:18 */

	uint32	tx_tries;	/* txdesc_13 */
	uint32	tx_rates;	/* txdesc_14 */

	uint32	rts_cts_0_1;	/* txdesc_15 */
	uint32	rts_cts_2_3;	/* txdese_16 */

	uint32	chain_sel;	/* txdesc_18 */
	uint32	ness_0;		/* txdesc_19 */
	uint32	ness_1;		/* txdesc_20 */
	uint32	ness_2;		/* txdesc_21 */
	uint32	ness_3;		/* txdesc_22 */
	/* txbf parameters */
} __attribute__((packed)) ath_wfo_mac_entry_t;
#endif

/* hal/ah.h */
#define CS_ATH_TXQ_INACT	0
#define CS_ATH_TXQ_DATA		1
#define CS_ATH_TXQ_BEACON	2
#define CS_ATH_TXQ_CAB		3
#define CS_ATH_TXQ_UAPSD	4
#define CS_ATH_TXQ_PSPOLL	5
#define CS_ATH_TXQ_CFEND	6
#define CS_ATH_TXQ_PAPRD	7
#define CS_ATH_AR9300_TXQ	8
typedef struct ar_cs_wfo_tx {
	void *	tx_desc_base;
	uint32	tx_paddr;
	uint8	sw_tx_index;
	uint8	hw_finish_index;
	uint16	txd_seq;

	void *txs_start;
	uint8	txs_index;
	uint32	txs_spaddr;
	uint32 txs_epaddr;
} ar_cs_wfo_tx_t;


/* flags passed to tx descriptor setup methods */
#define TXDESC_FLAG_CLRDMASK     0x0001	/* clear destination filter mask */
#define TXDESC_FLAG_NOACK        0x0002	/* don't wait for ACK */
#define TXDESC_FLAG_RTSENA       0x0004	/* enable RTS */
#define TXDESC_FLAG_CTSENA       0x0008	/* enable CTS */
#define TXDESC_FLAG_INTREQ       0x0010	/* enable per-descriptor interrupt */
#define TXDESC_FLAG_VEOL         0x0020	/* mark virtual EOL */
#define TXDESC_FLAG_EXT_ONLY     0x0040	/* send on ext channel only */
#define TXDESC_FLAG_EXT_AND_CTL  0x0080	/* send on ext + ctl channels */
#define TXDESC_FLAG_VMF          0x0100	/* virtual more frag */
#define TXDESC_FLAG_FRAG_IS_ON   0x0200	/* Fragmentation enabled */
#define TXDESC_FLAG_LOWRXCHAIN   0x0400	/* switch to low rx chain */


#endif



#endif
