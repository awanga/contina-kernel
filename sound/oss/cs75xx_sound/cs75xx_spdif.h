#ifndef _CS75XX_SPDIF_H
#define _CS75XX_SPDIF_H

#define SYS_REF_CLOCK	100000000

#define DEF_BUF_NUM	16
#define DEF_BUF_SIZE	4096

typedef struct {
	unsigned int cmd;
	unsigned int data;
} spdif_diag_cmd_t;

typedef struct {
	unsigned int nSamplesPerSec;
	unsigned int nDataLen;
	unsigned short format;
	unsigned short nChannels;
	unsigned short wBitsPerSample;
} wave_file_info_t;

typedef struct {
	unsigned short syncword;
	unsigned short crc1;
	unsigned char  frmsizecod : 6;
	unsigned char  fscod      : 2;
	unsigned char  bsmod      : 3;
	unsigned char  bsid       : 5;
	unsigned char  dsurmod    : 2;
	unsigned char  surmixlev  : 2;
	unsigned char  cmixlev    : 2;
	unsigned char  acmod      : 3;
} ac3_file_info_t;

typedef struct {
	unsigned short samplerate;
	unsigned short framesize;
	unsigned char type;
	unsigned char format;
} dts_file_info_t;

typedef enum {
	AUDIO_WAV,
	AUDIO_AC3,
	AUDIO_DTS,
	AUDIO_UNDEF
} spdif_audio_type_t;

#define SPDIF_DIAG_CMD			_IOW  ('q', 0x00, spdif_diag_cmd_t)
#define SPDIF_INIT_BUF			_IO   ('q', 0x01)
#define SPDIF_FILE_LEN			_IOW  ('q', 0x02, int)
#define SPDIF_STOP_PLAY			_IO   ('q', 0x03)
#define SPDIF_WAVE_FILEINFO		_IOW  ('q', 0x04, wave_file_info_t)
#define SPDIF_AC3_FILEINFO		_IOW  ('q', 0x05, ac3_file_info_t)
#define SPDIF_DTS_FILEINFO		_IOW  ('q', 0x06, dts_file_info_t)

enum diagcmd {
	DIAG_PCM_DATA,
	DIAG_NONPCM_DATA,
	DIAG_DMA_MODE,
	DIAG_INTER_CLOCK,
	DIAG_PREAMBLE_MODE,
	DIAG_DUMP_INFO,
	DIAG_CLOCK_GEN,
	DIAG_NONPCM_AC3,
};
#endif //_CS75XX_SPDIF_H

