//#include <stdio.h>
#include "cwda15.h"
//#include "cwsystem.h"

unsigned char CWda15_configuration_bits_array[8] = {0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05};

int CWda15_Identify(CWda15_Config_Type *InstancePtr){
    unsigned int ipcoreid;
    ipcoreid     = CWda15_READ_REG(InstancePtr->RegistersBaseAddress, CWda15_IP_CORE_VERSION);

    if (ipcoreid == 0xDA0F0300)
        return 1;
    else
        return 0;
}

void CWda15_StartSpdif(CWda15_Config_Type *InstancePtr){
    unsigned int conf;
    conf   =  CWda15_READ_REG(InstancePtr->RegistersBaseAddress, CWda15_CONFIGURATION_REGISTER);
    conf   |=  CWda15_ENABLE_SPDIF;
    CWda15_WRITE_REG(InstancePtr->RegistersBaseAddress, CWda15_CONFIGURATION_REGISTER, conf);
}

void CWda15_StopSpdif(CWda15_Config_Type *InstancePtr){
    unsigned int conf;
    conf   =  CWda15_READ_REG(InstancePtr->RegistersBaseAddress, CWda15_CONFIGURATION_REGISTER);
    conf   &=  ~CWda15_ENABLE_SPDIF;
    CWda15_WRITE_REG(InstancePtr->RegistersBaseAddress, CWda15_CONFIGURATION_REGISTER, conf);
}

void CWda15_StartData(CWda15_Config_Type *InstancePtr){
    unsigned int conf;
    conf   =  CWda15_READ_REG(InstancePtr->RegistersBaseAddress, CWda15_CONFIGURATION_REGISTER);
    conf   |=  CWda15_ENABLE_DATA;
    CWda15_WRITE_REG(InstancePtr->RegistersBaseAddress, CWda15_CONFIGURATION_REGISTER, conf);
}

void CWda15_StopData(CWda15_Config_Type *InstancePtr){
    unsigned int conf;
    conf   =  CWda15_READ_REG(InstancePtr->RegistersBaseAddress, CWda15_CONFIGURATION_REGISTER);
    conf   &=  ~CWda15_ENABLE_DATA;
    CWda15_WRITE_REG(InstancePtr->RegistersBaseAddress, CWda15_CONFIGURATION_REGISTER, conf);
}

/* Because compiler environment can't support micro AUDIO_MCLK_FREQ_DIVIDER(system_clk,
   sample_rate), use the function to caculate clock divider */
int CWda15_StartClock(CWda15_Config_Type *InstancePtr, unsigned int sample_rate, unsigned int clk_target){
#ifdef CONFIG_CORTINA_FPGA
	unsigned int clock = 0x008DB92D;	/* for 44.1K */

	switch (sample_rate) {
		case 8000:
			clock = 0x030D4000;	/* 100M/(8K*256) */
			break;
		case 11025:
			clock = 0x0236E4B7;	/* 100M/(11.025K*256) */
			break;
		case 12000:
			clock = 0x0208D555;	/* 100M/(12K*256) */
			break;
		case 16000:
			clock = 0x0186A000;	/* 100M/(16K*256) */
			break;
		case 22050:
			clock = 0x011B725B;	/* 100M/(22.05K*256) */
			break;
		case 24000:
			clock = 0x01046AAA;	/* 100M/(24K*256) */
			break;
		case 32000:
			clock = 0x00C35000;	/* 100M/(32K*256) */
			break;
		case 44100:
			clock = 0x008DB92D;	/* 100M/(44.1K*256) */
			break;
		case 48000:
			clock = 0x00823555;	/* 100M/(48K*256) */
			break;
		case 88200:
			clock = 0x0046DC96;	/* 100M/(88.2K*256) */
			break;
		case 96000:
			clock = 0x00411AAA;	/* 100M/(96K*256) */
			break;
		case 176400:
			clock = 0x00236E4B;	/* 100M/(176.4K*256) */
			break;
		case 192000:
			clock = 0x00208D55;	/* 100M/(192K*256) */
			break;
		default:
			return -1;//printk("Not support sample rate %d!\n", sample_rate);
	}
#else
	unsigned int clock;

	switch (clk_target)  {
	case 0:	/* SPDIF */
		switch (sample_rate) {
		case 8000:
			clock = 0x061A8000;	/* 200M/(8K*256) */
			break;
		case 11025:
			clock = 0x046DC96E;	/* 200M/(11.025K*256) */
			break;
		case 12000:
			clock = 0x0411AAAA;	/* 200M/(12K*256) */
			break;
		case 16000:
			clock = 0x030D4000;	/* 200M/(16K*256) */
			break;
		case 22050:
			clock = 0x0236E4B7;	/* 200M/(22.05K*256) */
			break;
		case 24000:
			clock = 0x0208D555;	/* 200M/(24K*256) */
			break;
		case 32000:
			clock = 0x0186A000;	/* 200M/(32K*256) */
			break;
		case 44100:
			clock = 0x011B725B;	/* 200M/(44.1K*256) */
			break;
		case 48000:
			clock = 0x01046AAA;	/* 200M/(48K*256) */
			break;
		case 88200:
			clock = 0x008E0BA2;	/* 200M/(88.2K*256) */
			break;
		case 96000:
			clock = 0x00823555;	/* 200M/(96K*256) */
			break;
		case 176400:
			clock = 0x004705D1;	/* 200M/(176.4K*256) */
			break;
		case 192000:
			clock = 0x00411AAA;	/* 200M/(192K*256) */
			break;
		default:
			return -1;//printk("Not support sample rate %d!\n", sample_rate);
		}
		break;
	case 1: /* I2S */
		switch (sample_rate) {
		case 8000:
			clock = 0x186A0000;	/* 200M/(8K*64) */
			break;
		case 11025:
			clock = 0x11B725BB;	/* 200M/(11.025K*64) */
			break;
		case 12000:
			clock = 0x1046AAAA;	/* 200M/(12K*64) */
			break;
		case 16000:
			clock = 0x0C350000;	/* 200M/(16K*64) */
			break;
		case 22050:
			clock = 0x08DB92DD;	/* 200M/(22.05K*64) */
			break;
		case 24000:
			clock = 0x08235555;	/* 200M/(24K*64) */
			break;
		case 32000:
			clock = 0x061A8000;	/* 200M/(32K*64) */
			break;
		case 44100:
			clock = 0x046DC96E;	/* 200M/(44.1K*64) */
			break;
		case 48000:
			clock = 0x0411AAAA;	/* 200M/(48K*64) */
			break;
		case 88200:
			clock = 0x0236E4B7;	/* 200M/(88.2K*64) */
			break;
		case 96000:
			clock = 0x0208DDDD;	/* 200M/(96K*64) */
			break;
		case 176400:
			clock = 0x011B725B;	/* 200M/(176.4K*64) */
			break;
		case 192000:
			clock = 0x01046AAA;	/* 200M/(192K*64) */
			break;
		default:
			return -1;//printk("Not support sample rate %d!\n", sample_rate);
		}
		break;
	case 2: /* PCM */
		switch (sample_rate) {
		case 8000:
			clock = 0x0C350000;	/* 200M/(8K*16*8) */
		default:
			return -1;//printk("Not support sample rate %d!\n", sample_rate);
		}
		break;
	default:
		return -1;//printk("%s: Not Support Clock Target\n", __func__);
	}
#endif

	CWda15_WRITE_REG(InstancePtr->RegistersBaseAddress, CWda15_CLOCK_GENERATOR, CWda15_INTERNAL_CLOCK | clock);
}

void CWda15_StopClock(CWda15_Config_Type *InstancePtr){
    unsigned int clock;
    clock   =  CWda15_READ_REG(InstancePtr->RegistersBaseAddress, CWda15_CLOCK_GENERATOR);
    clock   &=  ~CWda15_INTERNAL_CLOCK;
    CWda15_WRITE_REG(InstancePtr->RegistersBaseAddress, CWda15_CLOCK_GENERATOR, clock);
}

int CWda15_Configure(CWda15_Config_Type *InstancePtr){
    unsigned int data_conf = 0;
	unsigned int value = 0;

    if (InstancePtr->PcmMode){
        data_conf    = CWDA15_VV|CWDA15_RLAST;
    }
    else{
        data_conf 	  = CWDA15_NONPCM;
        data_conf 	|= (InstancePtr->framePeriod                      	& 0x00000FFF) << 20;
        data_conf 	|= (InstancePtr->pausePeriod                      	& 0x0000003F) << 10;
    }

    CWda15_WRITE_REG(InstancePtr->RegistersBaseAddress, CWda15_CONFIGURATION_REGISTER, data_conf);
#if 0
	if (InstancePtr->SampleFrequency > 0)
		value = AUDIO_MCLK_FREQ_DIVIDER(InstancePtr->SysClockFrequency,InstancePtr->SampleFrequency);
	else
		value = 0;

	CWda15_WRITE_REG(InstancePtr->RegistersBaseAddress, CWda15_CLOCK_GENERATOR, value);
#else
	if (InstancePtr->SampleFrequency > 0)
		CWda15_StartClock(InstancePtr, InstancePtr->SampleFrequency, 0);
	else
		CWda15_StopClock(InstancePtr);
#endif

    return 1;
}

void CWda15_Write_Audio_Sample(unsigned int baseaddress, unsigned int sample){
    int tmp=1;

    while ( tmp){
        tmp = CWda15_READ_REG(baseaddress, CWda15_FIFO_LEVEL);
        tmp = tmp & CWda15_FIFO_FULL;
    }
    CWda15_WRITE_REG(baseaddress, CWda15_WRITE_DATA_OFFSET, sample);
}

int CWda15_Configure_Buffers(CWda15_Config_Type *CWda15_Peripheral)
{
    if (CWda15_Peripheral->PcmMode) {
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0x0,     0x765ab211);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0x4,     0xfed89a98);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0x8,     0x76567210);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0xC,     0xfed45a98);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0x10,    0x76542310);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0x14,    0xfed01a98);

	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0x0,     0xabdcba90);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0x4,     0x89543210);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0x8,     0x67dcba98);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0xC,     0x45543210);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0x10,    0x23dcba98);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0x14,    0x01543218);

	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0x0,       0x32101234);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0x4,       0xba987654);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0x8,       0x3210fedc);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0xC,       0xba987654);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0x10,      0x3210fedc);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0x14,      0xba987654);

	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0x0,       0xbcba9878);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0x4,       0x3456789a);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0x8,       0x54321012);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0xC,       0xdcba9876);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0x10,      0x9abcdefe);
	CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0x14,      0x12345678);
   	}
	else {
		//CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0x0,     0x02109242);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0x0, 	0x02000006);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0x4,     0);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0x8,     0);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0xC,     0);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0x10,    0);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0x14,    0);

		//CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0x0,     0x02209242);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0x0, 	0x02000006);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0x4,     0);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0x8,     0);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0xC,     0);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0x10,    0);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0x14,    0);

		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0x0,		0x32101234);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0x4,		0xba987654);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0x8,		0x3210fedc);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0xC,		0xba987654);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0x10,		0x3210fedc);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0x14,		0xba987654);

		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0x0,		0xbcba9878);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0x4,		0x3456789a);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0x8,		0x54321012);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0xC,		0xdcba9876);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0x10,		0x9abcdefe);
		CWda15_WRITE_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0x14,		0x12345678);
	}

	return (1);
}

int CWda15_Dump_Registers(CWda15_Config_Type *CWda15_Peripheral, unsigned int *buf)
{
	*buf = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_DATA_REGISTER + 0x0);
	*(buf + 1) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_DATA_REGISTER + 0x4);

	*(buf + 2) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_DATA_REGISTER + 0x8);
	*(buf + 3) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_DATA_REGISTER + 0x10);
	*(buf + 4) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_DATA_REGISTER + 0x14);
	*(buf + 5) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_DATA_REGISTER + 0x18);
	*(buf + 6) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_DATA_REGISTER + 0x1C);
	*(buf + 7) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_DATA_REGISTER + 0x20);

	return (1);
}

int CWda15_Dump_Buffers(CWda15_Config_Type *CWda15_Peripheral, unsigned int *buf)
{
	*buf = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0x0);
	*(buf + 1) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0x4);

	*(buf + 2) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0x8);
	*(buf + 3) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0xC);
	*(buf + 4) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0x10);
	*(buf + 5) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_STATUS_BITS_BUFFER + 0x14);

	*(buf + 6) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0x0);
	*(buf + 7) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0x4);
	*(buf + 8) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0x8);
	*(buf + 9) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0xC);

	*(buf + 10) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0x10);
	*(buf + 11) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_STATUS_BITS_BUFFER + 0x14);

	*(buf + 12) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0x0);
	*(buf + 13) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0x4);
	*(buf + 14) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0x8);
	*(buf + 15) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0xC);
	*(buf + 16) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0x10);
	*(buf + 17) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL1_USER_BITS_BUFFER + 0x14);

	*(buf + 18) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0x0);
	*(buf + 19) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0x4);
	*(buf + 20) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0x8);
	*(buf + 21) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0xC);
	*(buf + 22) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0x10);
	*(buf + 23) = CWda15_READ_REG(CWda15_Peripheral->RegistersBaseAddress, CWda15_CHANNEL2_USER_BITS_BUFFER + 0x14);

	return (1);
}

int CWda15_Clear_Interrupt(CWda15_Config_Type *InstancePtr)
{
    unsigned int val;

    val = CWda15_READ_REG(InstancePtr->RegistersBaseAddress, CWda15_INTERRUPT_STATE_REGISTER);

	CWda15_WRITE_REG(InstancePtr->RegistersBaseAddress, CWda15_INTERRUPT_STATE_REGISTER, val);

	return val;
}

int CWda15_Dump_Interrupt(CWda15_Config_Type *InstancePtr)
{
    unsigned int val;

    val = CWda15_READ_REG(InstancePtr->RegistersBaseAddress, CWda15_INTERRUPT_STATE_REGISTER);

	//CWda15_WRITE_REG(InstancePtr->RegistersBaseAddress, CWda15_INTERRUPT_STATE_REGISTER, val);

	return val;
}

//~ int CWda15_Configure_Interrupt(CWda15_Config_Type *InstancePtr,  int interrupt_mask)
//~ {
    //~ unsigned int fifosize;
    //~ unsigned int interrupt_mask = 0;

    //~ CWda15_WRITE_REG(InstancePtr->RegistersBaseAddress, CWda15_INTERRUPT_STATE_REGISTER,interrupt_mask);

//~ }

