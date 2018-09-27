#ifndef CWda15_H
#define CWda15_H

#define CWda15_WRITE_DATA_OFFSET                0x0000

#define CWda15_DATA_REGISTER                    0x0000
#define CWda15_CONFIGURATION_REGISTER       	0x0004
#define CWda15_INTERRUPT_STATE_REGISTER         0x0008
#define CWda15_IP_CORE_VERSION                	0x0010
#define CWda15_FIFO_LEVEL                       0x0014
#define CWda15_FIFO_SIZE                        0x0018
#define CWda15_FIFO_LOWER_LIMIT              	0x001C
#define CWda15_CLOCK_GENERATOR                  0x0020
#define CWda15_CHANNEL1_STATUS_BITS_BUFFER      0x0080
#define CWda15_CHANNEL2_STATUS_BITS_BUFFER      0x00A0
#define CWda15_CHANNEL1_USER_BITS_BUFFER        0x00C0
#define CWda15_CHANNEL2_USER_BITS_BUFFER        0x00E0

#define CWda15_Io_In32(InputPtr)                (*(volatile unsigned int *)(InputPtr))
#define CWda15_Io_Out32(OutputPtr, Value)       { (*(volatile unsigned int *)(OutputPtr) = Value);  }

#define CWda15_READ_DATA(baseaddr)              CWda15_Io_In32((baseaddr) + (CWda15_READ_DATA_OFFSET))
#define CWda15_WRITE_DATA(baseaddr, value)      CWda15_Io_Out32((baseaddr) + (CWda15_WRITE_DATA_OFFSET), (value))
#define CWda15_READ_REG(baseaddr, reg)          CWda15_Io_In32((baseaddr) + (reg))
#define CWda15_WRITE_REG(baseaddr, reg, value)  CWda15_Io_Out32((baseaddr) + (reg), (value))

#define CWda15_FIFO_FULL                        0x20000000
#define CWda15_INTERNAL_CLOCK                   0x80000000

#define CWda15_ENABLE_SPDIF                     (0x01 << 8)
#define CWda15_ENABLE_DATA                      (0x01 << 9)

#define CWDA15_RLAST    0x00000010
#define CWDA15_VV       0x00010000
#define CWDA15_NONPCM   0x00020000

#define AUDIO_MCLK_FREQ_DIVIDER(CLK, FS)  (((int)((((long long)(CLK))<<20)/(256*(FS)))) | 0x80000000)

typedef struct
{
    unsigned int  DataBaseAddress;
    unsigned int  RegistersBaseAddress;

    unsigned char PcmMode;
    unsigned int  burstLength;
    unsigned int  framePeriod;
    unsigned int  pausePeriod;
    unsigned int  SysClockFrequency;
    unsigned int  SampleFrequency;

    int            *Current_InBuffer;
    int            *Begin_InBuffer;
    int            *End_InBuffer;

} CWda15_Config_Type;


int CWda15_Identify(CWda15_Config_Type *InstancePtr);

int CWda15_Configure(CWda15_Config_Type *InstancePtr);

void CWda15_Start(CWda15_Config_Type *InstancePtr);

void CWda15_StartSpdif(CWda15_Config_Type *InstancePtr);

void CWda15_StopSpdif(CWda15_Config_Type *InstancePtr);

void CWda15_StartData(CWda15_Config_Type *InstancePtr);

void CWda15_StopData(CWda15_Config_Type *InstancePtr);

int CWda15_StartClock(CWda15_Config_Type *InstancePtr, unsigned int sample_rate, unsigned int clk_target);

void CWda15_StopClock(CWda15_Config_Type *InstancePtr);

void CWda15_Write_Audio_Sample(unsigned int baseaddress, unsigned int sample);

int CWda15_Configure_Buffers(CWda15_Config_Type *CWda15_Peripheral);

int CWda15_Dump_Registers(CWda15_Config_Type *CWda15_Peripheral, unsigned int *buf);

int CWda15_Dump_Buffers(CWda15_Config_Type *CWda15_Peripheral, unsigned int *buf);

int CWda15_Clear_Interrupt(CWda15_Config_Type *InstancePtr);

int CWda15_Dump_Interrupt(CWda15_Config_Type *InstancePtr);

//int CWda15_Configure_Interrupt(CWda15_Config_Type *InstancePtr,  int interrupt_mask)

#endif
