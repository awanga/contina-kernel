// ---------------------------------------------------------------------------
// Analogix Confidential Strictly Private
//
// $RCSfile: DP_TX_DRV.h,v $
// $Revision: 1.2 $
// $Author: sjlin $
// $Date: 2012/03/15 06:37:35 $
//
// ---------------------------------------------------------------------------
// >>>>>>>>>>>>>>>>>>>>>>>>> COPYRIGHT NOTICE <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// ---------------------------------------------------------------------------
// Copyright 2004-2007 (c) Analogix 
//
//Analogix owns the sole copyright to this software. Under international
// copyright laws you (1) may not make a copy of this software except for
// the purposes of maintaining a single archive copy, (2) may not derive
// works herefrom, (3) may not distribute this work to others. These rights
// are provided for information clarification, other restrictions of rights
// may apply as well.
//
// This is an unpublished work.
// ---------------------------------------------------------------------------
// >>>>>>>>>>>>>>>>>>>>>>>>>>>> WARRANTEE <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// ---------------------------------------------------------------------------
// Analogix  MAKES NO WARRANTY OF ANY KIND WITH REGARD TO THE USE OF
// THIS SOFTWARE, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR
// PURPOSE.
// ---------------------------------------------------------------------------

#ifndef _DP_TX_DRV_H
#define _DP_TX_DRV_H

#define DP_HDMI_TX_FW_VER 1.22


#define CR_LOOP_TIME 5
#define EQ_LOOP_TIME 5

typedef unsigned char BYTE;
//typedef bit BIT;
typedef unsigned int WORD;

typedef unsigned char *pByte;



#define P0_0  0x80
#define P0_1  0x81
#define P0_2  0x82
#define P0_3  0x83
#define P0_4  0x84
#define P0_5  0x85
#define P0_6  0x86
#define P0_7  0x87

#define P1_0  0x90
#define P1_1  0x91
#define P1_2  0x92
#define P1_3  0x93
#define P1_4  0x94
#define P1_5  0x95
#define P1_6  0x96
#define P1_7  0x97

#define P2_0  0xa0
#define P2_1  0xa1
#define P2_2  0xa2
#define P2_3  0xa3
#define P2_4  0xa4
#define P2_5  0xa5
#define P2_6  0xa6
#define P2_7  0xa7

#define P3_0  0xb0
#define P3_1  0xb1
#define P3_2  0xb2
#define P3_3  0xb3
#define P3_4  0xb4
#define P3_5  0xb5
#define P3_6  0xb6
#define P3_7  0xb7

#define ACC_0  0xe0
#define ACC_1  0xe1
#define ACC_2  0xe2
#define ACC_3  0xe3
#define ACC_4  0xe4
#define ACC_5  0xe5
#define ACC_6  0xe6
#define ACC_7  0xe7

#define _NOP_	_nop_()
#define PUTCHAR_TYPE	char

//#define BIST_EN P1_4
#define BIST_EN 0				// Joe20110922

#define DP_TX_Resetn_Pin P3_2
#define DP_TX_Hotplug_pin P3_4
#define DP_TX_interrupt_pin ~P3_3
#define DP_TX_Dev_Sel 1

//#define SWITCH1 P0_3
#define SWITCH1 0	// Joe20111011
#define SWITCH2 P0_2
#define SWITCH3 P0_1
//#define SWITCH4 P0_0
#define SWITCH4 0	// Joe20110923

#define Force_Video_Resolution P0_0
	
/*
#define COMMAND_CODE_INT_SOURCE                 0x00
#define COMMAND_CODE_GET_DEVICE_ID              0x01
#define COMMAND_CODE_IS_SINK_DEVICE_PRESENT     0x02
#define COMMAND_CODE_POWER_UP_DOWN_DP           0x03
#define COMMAND_CODE_ENABLE_LINK_TRAINING       0x04
#define COMMAND_CODE_GET_SINK_CAPABILITY        0x05
#define COMMAND_CODE_ACTIVE_DP                  0x06
#define COMMAND_CODE_BLANK_DP                   0x07
#define COMMAND_CODE_SET_VIDEO_TYPE             0x08
#define COMMAND_CODE_EDID_READ                  0x09
#define COMMAND_CODE_SET_INTERRUPT_MASK         0x0A

#define COMMAND_CODE_GET_SINK_HDCP_CAPABILITY   0x00
#define COMMAND_CODE_GET_SINK_BKSV              0x01
#define COMMAND_CODE_GET_DOWN_STREAM_KSV_LIST   0x02
#define COMMAND_CODE_GET_SOURCE_AN              0x03
#define COMMAND_CODE_GET_SOURCE_AKSV            0x04
#define COMMAND_CODE_HDCP_AUTHENTICATION        0x05
#define COMMAND_CODE_HDCP_ENCRYPTION            0x06
#define COMMAND_CODE_IS_LINK_SECURED            0x07

#define COMMAND_CODE_RESET_DP                   0x00
#define COMMAND_CODE_SET_VIDEO_FORMAT           0x01
#define COMMAND_CODE_ENABLE_VIDEO_BIST          0x02
#define COMMAND_CODE_ENABLE_AVI_INFOR_FRAME     0x03
#define COMMAND_CODE_ENABLE_SPD_INFOR_FRAME     0x04
#define COMMAND_CODE_ENABLE_MPEG_INFOR_FRAME    0x05
#define COMMAND_CODE_DISABLE_INFOR_FRAME        0x06
#define COMMAND_CODE_WRITE_REGISTER             0x07
#define COMMAND_CODE_READ_REGISTER              0x08

#define COMMAND_FINISH 0x0f
#define COMMAND_NOT_FINISH 0x00
#define COMMAND_ERROR 0xff
*/ 
#undef	QUICK_BOOT_NO_CHECK
#define DISPLAYPORT_SINK 0
#define DVI_SINK 1
	
#define CHIP_TYPE DISPLAYPORT_SINK
	
#define hdmi_tx_V640x480p_60Hz 1
#define hdmi_tx_V720x480p_60Hz_4x3 2
#define hdmi_tx_V720x480p_60Hz_16x9 3
#define hdmi_tx_V1280x720p_60Hz 4
#define hdmi_tx_V1280x720p_50Hz 19
#define hdmi_tx_V1920x1080i_60Hz 5
#define hdmi_tx_V1920x1080p_60Hz 16
#define hdmi_tx_V1920x1080p_50Hz 31
#define hdmi_tx_V1920x1080i_50Hz 20
#define hdmi_tx_V720x480i_60Hz_4x3 6
#define hdmi_tx_V720x480i_60Hz_16x9 7
#define hdmi_tx_V720x576i_50Hz_4x3 21
#define hdmi_tx_V720x576i_50Hz_16x9 22
#define hdmi_tx_V720x576p_50Hz_4x3 17
#define hdmi_tx_V720x576p_50Hz_16x9 18
extern BYTE hdmi_tx_video_id;
extern BYTE hdmi_tx_in_pix_rpt, hdmi_tx_tx_pix_rpt;
typedef struct  {
	BYTE type;
	BYTE version;
	BYTE length;
	BYTE pb_byte[28];
} HDMI_INFOFRAME_STRUCT;
typedef enum  { HDMI_avi_infoframe, HDMI_audio_infoframe, 
		//HDMI_USRDF0_infoframe,
		//HDMI_USRDF1_infoframe,
	
} HDMI_PACKET_TYPE;
typedef enum  { 
	RGB=0, RBG, GRB, BRG, GBR, BGR, NO_MAP,
} RGB_ORD;	// RGB order
typedef struct  {
	BYTE packets_need_config;	//which infoframe packet is need updated
	HDMI_INFOFRAME_STRUCT avi_info;
	HDMI_INFOFRAME_STRUCT audio_info;
	
		/*  for the funture use
		   HDMI_INFOFRAME_STRUCT spd_info;
		   HDMI_INFOFRAME_STRUCT mpeg_info;
		   HDMI_INFOFRAME_STRUCT acp_pkt;
		   HDMI_INFOFRAME_STRUCT isrc1_pkt;
		   HDMI_INFOFRAME_STRUCT isrc2_pkt;
		   HDMI_INFOFRAME_STRUCT vendor_info; */ 
} HDMI_CONFIG_PACKETS;
extern HDMI_CONFIG_PACKETS Hdmi_packet_config;

//extern BYTE   ByteBuf[MAX_BUF_CNT];
	
#define HDMI_TX_avi_sel 0x01
#define HDMI_TX_audio_sel 0x02
typedef enum  { AVI_PACKETS, SPD_PACKETS, MPEG_PACKETS 
} PACKETS_TYPE;
struct Packet_AVI {
	BYTE AVI_data[15];
};
struct Packet_SPD {
	BYTE SPD_data[27];
};
struct Packet_MPEG {
	BYTE MPEG_data[27];
};
struct Bist_Video_Format {
	char number;
	char video_type[32];
	unsigned int pixel_frequency;
	unsigned int h_total_length;
	unsigned int h_active_length;
	unsigned int v_total_length;
	unsigned int v_active_length;
	unsigned int h_front_porch;
	unsigned int h_sync_width;
	unsigned int h_back_porch;
	unsigned int v_front_porch;
	unsigned int v_sync_width;
	unsigned int v_back_porch;
	unsigned char h_sync_polarity;
	unsigned char v_sync_polarity;
	unsigned char is_interlaced;
	unsigned char pix_repeat_times;
	unsigned char frame_rate;
	unsigned char bpp_mode;
	unsigned char video_mode;
};

#define DP_TX_AVI_SEL 0x01
#define DP_TX_SPD_SEL 0x02
#define DP_TX_MPEG_SEL 0x04
	
#define HDMI_TX_N_32k 0x1000
#define HDMI_TX_N_44k 0x1880
#define HDMI_TX_N_88k 0x3100
#define HDMI_TX_N_176k 0x6200
#define HDMI_TX_N_48k 0x1800
#define HDMI_TX_N_96k 0x3000
#define HDMI_TX_N_192k 0x6000
extern BYTE dp_tx_video_id;
extern BYTE colordepth_packet_mode;
extern BYTE checksum;
extern BYTE dp_tx_hdcp_enable, dp_tx_ssc_enable;
extern BYTE USE_FW_LINK_TRAINING;
extern BYTE RST_ENCODER, mode_hdmi;
extern BYTE mode_dp;			//
extern BYTE dp_tx_lane_count;
extern BYTE bForceSelIndex;

//extern BYTE video_bpc;
extern BYTE bBW_Lane_Adjust;
typedef enum { DP_TX_INITIAL = 1, DP_TX_WAIT_HOTPLUG, DP_TX_PARSE_EDID, DP_TX_LINK_TRAINING,
	DP_TX_CONFIG_VIDEO, DP_TX_HDCP_AUTHENTICATION,
	DP_TX_CONFIG_AUDIO, DP_TX_PLAY_BACK 
} DP_TX_System_State;
typedef enum  { VIP_CSC_RGB, VIP_CSC_YCBCR422, VIP_CSC_YCBCR444 
} Video_Input_CSC;
typedef enum  { COLOR_6, COLOR_8, COLOR_10, COLOR_12 
} VIP_COLOR_DEPTH;
struct VideoFormat  {
	VIP_COLOR_DEPTH bColordepth;
	Video_Input_CSC bColorSpace;
};

#define DATA_RATE_SDR 0
#define DATA_RATE_DDR 1
	
#define PIXEL_PER_CYCLE_1 0
#define PIXEL_PER_CYCLE_2 1
	
#define CAPTURE_EDGE_RAISING 0
#define CAPTURE_EDGE_FALLING 1
	
#define init_timer_slot()   do { timer_slot = 0; } while (0)
	
#define EDID_Dev_ADDR 0xa0
#define EDID_SegmentPointer_ADDR 0x60
	
#define DP_TX_PORT0_ADDR 0x70
#define DP_TX_PORT1_ADDR 0x74
#define HDMI_TX_PORT0_ADDR 0x72
#define HDMI_TX_PORT1_ADDR 0x7A
	
#define DP_TX_HDCP_FAIL_THRESHOLD 10
extern BYTE dp_tx_system_state;
extern BYTE timer_slot;
extern struct VideoFormat DP_TX_Video_Input;

// Function Declaration by SJ 20120313: Start 
u8 DP_TX_Read_Reg(BYTE I2cSlaveAddr, BYTE RegAddr, BYTE *DataPtr);
u8 DP_TX_Write_Reg(BYTE I2cSlaveAddr, BYTE RegAddr, BYTE RegData);
// Function Declaration by SJ 20120313: End

void DP_TX_Task(void);
void DP_TX_TimerProcess(void);
void DP_TX_Int_Process(void);
void DP_TX_Initialization(void);
void DP_TX_Variable_Init(void);
void DP_TX_Set_System_State(BYTE ss);
void DP_TX_Timer_Slot1(void);
void DP_TX_Timer_Slot2(void);
void DP_TX_Timer_Slot3(void);
void PCLK_Calc(BYTE hbr_rbr);
void BW_LC_Sel(void);
void Video_Timing(void);
void video_bit_ctrl(BYTE colorcomp);
void EnhacedMode_Clear(void);
void DP_TX_FORCE_HPD(void);
void DP_TX_UNFORCE_HPD(void);
void DP_TX_BIST_Format_Config(WORD dp_tx_bist_select_number);
void DP_TX_BIST_Format_Resolution(WORD dp_tx_bist_select_number);
void DP_TX_BIST_Config_CLK_Genarator(WORD dp_tx_bist_select_number);
void DP_TX_BIST_Clk_MN_Gen(WORD dp_tx_bist_select_number);

//void DP_TX_Parse_EDID (void);
void DP_TX_Link_Training(void);
void DP_TX_Config_Video(void);
void DP_TX_HDCP_Process(void);
void DP_TX_PlayBack_Process(void);

/*BYTE DP_TX_Read_EDID (void);
BYTE DP_TX_Parse_EDID_Header (void);
void DP_TX_Parse_EDID_Established_Timing (void);
void DP_TX_Parse_EDID_Standard_Timing (void);
void DP_TX_Parse_EDID_Detailed_Timing (void);
void DP_TX_Parse_DTD(BYTE DTD_Block);*/ 
void DP_TX_Config_Video(void);

//void DP_TX_Embedded_Sync(void);
//void DP_TX_DE_reGenerate (void);
void DP_TX_Show_Infomation(void);
void DP_TX_HPD_Int_Handler(BYTE hpd_source);

// void DP_TX_Enable_Vid_Input(void);
void DP_TX_Video_Changed_Handler(void);
void DP_TX_PLL_Changed_Int_Handler(void);

//void Config_Color_Depth(void);
void DP_TX_Power_Down(void);
void DP_TX_Power_On(void);
void DP_TX_Enable_Video_Input(void);

//void DP_TX_LinkTraining_Again(void);
//BYTE DP_TX_AUX_DPCDRead_Byte(BYTE addrh, BYTE addrm, BYTE addrl);
//BYTE DP_TX_AUX_DPCDWrite_Byte(BYTE addrh, BYTE addrm, BYTE addrl, BYTE data1);
void DP_TX_HDCP_Encryption_Disable(void);
void DP_TX_HDCP_Encryption_Enable(void);
void DP_TX_Blue_Screen_Enable(void);

//BYTE DP_TX_SRM_CHK(void);//Removed for not call
void DP_TX_HW_HDCP_Enable(void);

//void DP_TX_Load_Packet (void);
	
//#ifdef HDMI_COLOR_DEBUG
void ANX9805_VIDEO_Mapping_8(void);
void ANX9805_VIDEO_Mapping_10(void);
void ANX9805_VIDEO_Mapping_12(void);

//#endif
void DP_TX_Blue_Screen_Disable(void);

//void DP_TX_Int_Info(BYTE int_v, BYTE id);
void DP_TX_Clean_HDCP(void);
void DP_TX_PBBS7_Test(void);
void DP_TX_Insert_Err(void);
void DP_TX_EnhaceMode_Set(void);
BYTE DP_TX_LT_Pre_Config(void);
void DP_TX_CONFIG_SSC(void);
void DP_TX_Config_Audio(void);
void HDMI_Parse_DTD(void);
BYTE HDMI_Parse_CheckSum(void);
void DP_TX_RST_DDC(void);
BYTE DP_TX_Get_EDID_Block(void);
void DP_TX_AUX_EDIDRead_Byte(BYTE offset);
void DP_TX_LL_CTS_Test(void);
void DP_TX_EDID_Read(void);
void DP_TX_RST_AUX(void);

//#ifdef SW_LINKTRAINING
typedef enum { 
	LINKTRAINING_START, CLOCK_RECOVERY_PROCESS,
	EQ_TRAINING_PROCESS, LINKTRAINING_FINISHED 
} DP_SW_LINK_State;

void DP_TX_EQ_Process(void);
void DP_TX_CLOCK_Recovery_Process(void);
void DP_TX_HW_LT(BYTE bw, BYTE lc);
void DP_TX_Link_Start_Process(void);
void DP_TX_SW_LINK_Process(void);
void DP_TX_Set_Link_state(DP_SW_LINK_State eState);

//void DP_TX_LT_VS_EQ_Set(BYTE bLane,BYTE bLevel);
//#endif
BYTE DP_TX_AUX_DPCDRead_Bytes(long addr, BYTE cCount, pByte pBuf);
void DP_TX_Wait_AUX_Finished(void);
void DP_TX_AUX_WR(BYTE offset);
void DP_TX_AUX_RD(BYTE len_cmd);
void DP_TX_AUX_DPCDWrite_Bytes(long addr, BYTE cCount, pByte pBuf);
void HDCP_Auth_Done_Interrupt(void);
void HDCP_Auth_Change_Interrupt(void);
void DP_TX_CLEAR_AVMUTE(void);
void DP_TX_SET_AVMUTE(void);
void DP_TX_Video_Disable(void);

//void DP_TX_CHK_Symbol_Error(void);
BYTE Sink_Is_DP_HDMI(void);
void HDMI_TX_Parse_EDID(void);
void HDMI_Parse_STD(void);
BYTE HDMI_Read_EDID_BYTE(BYTE offset);
void HDMI_InitDDC_Read(void);
void HDMI_Parse_VendorSTD(BYTE std);
void HDMI_TX_CSCandColorDepth_Setting(void);
BYTE DP_TX_Chip_Located(void);
BYTE HDMI_Parse_EDIDHeader(void);
void HDMI_TX_RepeatTimes_Setting(void);
BYTE HDMI_TX_Config_Packet(void);
BYTE HDMI_TX_Load_Infoframe(HDMI_PACKET_TYPE member, HDMI_INFOFRAME_STRUCT * p);
BYTE HDMI_TX_Checksum(HDMI_INFOFRAME_STRUCT * p);
void HDMI_DDC_Read(BYTE devaddr, BYTE segmentpointer, 
					BYTE offset, BYTE access_num_Low, BYTE access_num_high);
void HDMI_RST_DDCChannel(void);
void HDMI_DDC_Set_Address(BYTE devaddr, BYTE segmentpointer, 
						   BYTE offset, BYTE access_num_Low, BYTE access_num_high);
void HDMI_GCP_PKT_Enable(void);
void HDMI_AIF_PKT_Enable(void);
void DP_TX_InputSet(BYTE bColorSpace, BYTE cBpc);

//-------------------------------------------------------------------
//Function:     DP_TX_Config_Packets
//
//DESCRIPTION: configure the packets
//RETURN:
//
//NOTE: 
//-------------------------------------------------------------------
void DP_TX_Config_Packets(PACKETS_TYPE bType);

//-------------------------------------------------------------------
//Function:     DP_TX_Load_Packet
//
//DESCRIPTION: load the packets
//RETURN:
//
//NOTE: 
//-------------------------------------------------------------------
void DP_TX_Load_Packet(PACKETS_TYPE type);

#endif /*  */
