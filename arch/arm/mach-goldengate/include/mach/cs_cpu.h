#ifndef __ASM_ARCH_CS_H__
#define __ASM_ARCH_CS_H__

#define CS_SOC_CS7522_A0	0x7522A0
#define CS_SOC_CS7522_A1	0x7522A1
#define CS_SOC_CS7542_A0	0x7542A0
#define CS_SOC_CS7542_A1	0x7542A1

#define CS_SOC_UNKNOWN		0xFFFFFF

#ifdef CONFIG_ARCH_GOLDENGATE
extern unsigned int cs_get_soc_type(void);

#define cs_soc_is_cs7522()	((cs_get_soc_type() == CS_SOC_CS7522_A0) || \
				 (cs_get_soc_type() == CS_SOC_CS7522_A1))
#define cs_soc_is_cs7522a0()	(cs_get_soc_type() == CS_SOC_CS7522_A0)
#define cs_soc_is_cs7522a1()	(cs_get_soc_type() == CS_SOC_CS7522_A1)
#define cs_soc_is_cs7542()	((cs_get_soc_type() == CS_SOC_CS7542_A0) || \
				 (cs_get_soc_type() == CS_SOC_CS7542_A1))
#define cs_soc_is_cs7542a0()	(cs_get_soc_type() == CS_SOC_CS7542_A0)
#define cs_soc_is_cs7542a1()	(cs_get_soc_type() == CS_SOC_CS7542_A1)
#else
#define cs_soc_is_cs7522()	(0)
#define cs_soc_is_cs7522a0()	(0)
#define cs_soc_is_cs7522a1()	(0)
#define cs_soc_is_cs7542()	(0)
#define cs_soc_is_cs7542a0()	(0)
#define cs_soc_is_cs7542a1()	(0)
#endif

#endif /*  __ASM_ARCH_CS_H__ */
