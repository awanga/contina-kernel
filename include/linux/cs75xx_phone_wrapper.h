#ifndef CS75XX_PHONE_WRAPPER_H
#define CS75XX_PHONE_WRAPPER_H

/* SLIC Wrapper API */
int cs_slic_hw_reset(void);
unsigned char *cs_slic_profile(unsigned int index);
unsigned int cs_slic_timeslot(unsigned int chan);
int cs_slic_read_byte(unsigned int slic_id, void *rx_param);
int cs_slic_write_byte(unsigned int slic_id, void *tx_param);
/* PCM Wrapper API */
int cs_pcm_total_chan(void);
unsigned int cs_pcm_valid_chan(void);
char* cs_pcm_tx_buf_req(void);
void cs_pcm_tx_buf_done(void);
char* cs_pcm_rx_buf_req(void);
void cs_pcm_rx_buf_done(void);
int cs_pcm_enable(void);
int cs_pcm_disable(void);
int cs_pcm_init(void (*tx_handler)(void), void (*rx_handler)(void));
int cs_pcm_exit(void);
/* freq notifier API */
int cs_pcm_dma_start(void);
int cs_pcm_dma_stop(void);
/* Misc wrapper API */
unsigned int cs_get_hw_timestamp(void);
unsigned int cs_get_hw_timestamp_delta(unsigned int t1, unsigned int t2);
int cs_slic_rw_word(uint slic_id, void *tx_param,void *rx_param,int len);
#endif		/* CS75XX_PHONE_WRAPPER_H */

