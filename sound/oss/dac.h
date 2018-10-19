#ifndef _DAC_H
#define _DAC_H

typedef struct {
	unsigned int nSamplesPerSec;
	unsigned int nDataLen;
	unsigned short format;
	unsigned short nChannels;
	unsigned short wBitsPerSample;
	unsigned short endian;
} wave_file_info_t;

#define MAX_BUFS     64		/* maximum number of rotating buffers */
#ifdef CONFIG_CORTINA_FPGA
#define SSP_BUF_SIZE 2048	/* needs to be a multiple of PAGE_SIZE (4096)! */
#else
#define SSP_BUF_SIZE 4096
#endif
#define LLP_SIZE     MAX_BUFS	//120//1536
#define SBUF_SIZE    SSP_BUF_SIZE	//2048//1024
#define DBUF_SIZE    SBUF_SIZE
#define TBUF_SIZE    (LLP_SIZE)*SBUF_SIZE


#define SSP_I2S_INIT_BUF			_IO  ('q', 0x00)
#define SSP_I2S_STOP_DMA			_IO  ('q', 0x01)
#define SSP_I2S_FILE_LEN			_IOW ('q', 0x02, int)
#define SSP_I2S_INIT_MIXER			_IO  ('q', 0x03)
#define SSP_I2S_INC_LEVEL			_IOW ('q', 0x04, int)
#define SSP_I2S_DEC_LEVEL			_IOW ('q', 0x05, int)
#define SSP_I2S_SETFMT				_IOW ('q', 0x06, int)
#define SSP_I2S_STEREO				_IOW ('q', 0x07, int)
#define SSP_I2S_SETSPEED			_IOW ('q', 0x08, int) //48000 44100
#define SSP_I2S_SETCHANNELS			_IOW ('q', 0x09, int)
#define SSP_I2S_FILE_INFO			_IOW ('q', 0x0A, wave_file_info_t)

#endif //_DAC_H

