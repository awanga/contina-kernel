/*
 *  Copyright (C) 2014 Cortina-Systems Limited
 *  Copyright (C) 2002 - 2005 Benjamin Herrenschmidt <benh@kernel.crashing.org>
 *  and                       Markus Demleitner <msdemlei@cl.uni-heidelberg.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This driver adds basic cpufreq support for CS754X SoC.
 */

#define pr_fmt(fmt) "cpufreq: " fmt

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/cpufreq.h>

#include <asm/mach-types.h>
#include <mach/cs_pwrmgt.h>
#include <mach/cs_cpu.h>

static struct cpufreq_frequency_table *goldengate_cpu_freqs,
	cs7522_goldengate_cpu_freqs[] = {
		{CS_CPU_FREQUENCY_400,	400 * 1000},
#ifdef CONFIG_G2_UNSUPPORTED_EXPERIMENTAL_CPU_FREQS
		{CS_CPU_FREQUENCY_600,	600 * 1000},
		{CS_CPU_FREQUENCY_700,	700 * 1000},
		{CS_CPU_FREQUENCY_750,	750 * 1000},
#endif /* CONFIG_G2_UNSUPPORTED_EXPERIMENTAL_CPU_FREQS */
		{0,			CPUFREQ_TABLE_END},
		},
	cs7542_goldengate_cpu_freqs[] = {
		{CS_CPU_FREQUENCY_400,	400 * 1000},
#ifdef CONFIG_G2_UNSUPPORTED_EXPERIMENTAL_CPU_FREQS
		{CS_CPU_FREQUENCY_600,	600 * 1000},
		{CS_CPU_FREQUENCY_700,	700 * 1000},
		{CS_CPU_FREQUENCY_750,	750 * 1000},
#endif /* CONFIG_G2_UNSUPPORTED_EXPERIMENTAL_CPU_FREQS */
		{CS_CPU_FREQUENCY_800,	800 * 1000},
#ifdef CONFIG_G2_UNSUPPORTED_EXPERIMENTAL_CPU_FREQS
		{CS_CPU_FREQUENCY_850,	850 * 1000},
		{CS_CPU_FREQUENCY_900,	900 * 1000},
#endif /* CONFIG_G2_UNSUPPORTED_EXPERIMENTAL_CPU_FREQS */
		{0,			CPUFREQ_TABLE_END},
	};

static struct freq_attr *goldengate_cpu_freqs_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static cs_dev_id_t dev_id;
static int pmode_cur;

static DEFINE_MUTEX(goldengate_switch_mutex);

/*
 * Common interface to the cpufreq core
 */
static int goldengate_cpufreq_verify(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, goldengate_cpu_freqs);
}

static int goldengate_cpufreq_target(struct cpufreq_policy *policy,
	unsigned int target_freq, unsigned int relation)
{
	unsigned int newstate = 0;
	struct cpufreq_freqs freqs;
	int rc;

	if (cpufreq_frequency_table_target(policy, goldengate_cpu_freqs,
			target_freq, relation, &newstate))
		return -EINVAL;

	if (pmode_cur == newstate)
		return 0;

	mutex_lock(&goldengate_switch_mutex);

	freqs.old = goldengate_cpu_freqs[pmode_cur].frequency;
	freqs.new = goldengate_cpu_freqs[newstate].frequency;
	freqs.cpu = 0;

	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
	rc = cs_pm_cpu_frequency_set(dev_id,
				     goldengate_cpu_freqs[newstate].index);
	pmode_cur = newstate;
	msleep(15);
	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	mutex_unlock(&goldengate_switch_mutex);

	return rc;
}

static unsigned int goldengate_cpufreq_get_speed(unsigned int cpu)
{
	/* At the moment we are the only user of Cortina CPU freq PM API, when
	   there is another user, this method won't be relevant anymore */
	return goldengate_cpu_freqs[pmode_cur].frequency;
}

static int goldengate_cpufreq_cpu_init(struct cpufreq_policy *policy)
{
	policy->cpuinfo.transition_latency = 5 * NSEC_PER_MSEC; /* 5ms */
	policy->cur = goldengate_cpu_freqs[pmode_cur].frequency;

	/* The frequency of the second CPU core is tied to the frequency of
	   the first CPU core */
	cpumask_copy(policy->cpus, cpu_online_mask);
	cpufreq_frequency_table_get_attr(goldengate_cpu_freqs, policy->cpu);

	return cpufreq_frequency_table_cpuinfo(policy, goldengate_cpu_freqs);
}

static struct cpufreq_driver goldengate_cpufreq_driver = {
	.name		= "goldengate",
	.owner		= THIS_MODULE,
	.flags		= CPUFREQ_CONST_LOOPS,
	.init		= goldengate_cpufreq_cpu_init,
	.verify		= goldengate_cpufreq_verify,
	.target		= goldengate_cpufreq_target,
	.get		= goldengate_cpufreq_get_speed,
	.attr		= goldengate_cpu_freqs_attr,
};

static int __init goldengate_cpufreq_init(void)
{
	int rc, max_index, i = 0;
	cs_pm_cpu_info_t cpu_info;

	if (cs_soc_is_cs7522()) {
		goldengate_cpu_freqs = cs7522_goldengate_cpu_freqs;
		max_index = ARRAY_SIZE(cs7522_goldengate_cpu_freqs) - 2;
	} else if (cs_soc_is_cs7542()) {
		goldengate_cpu_freqs = cs7542_goldengate_cpu_freqs;
		max_index = ARRAY_SIZE(cs7542_goldengate_cpu_freqs) - 2;
	} else {
		pr_info("machine SoC is not supported\n");
		return -ENODEV;
	}

	cs_pm_cpu_info_get(dev_id, &cpu_info);
	if (cpu_info.freq > CS_CPU_FREQUENCY_900) {
		pr_err("unsupported API frequency: %d\n", cpu_info.freq);
		return -ENODEV;
	}

	/* Translate Cortina PM API index to CPUfreq table index */
	while (goldengate_cpu_freqs[i].frequency != CPUFREQ_TABLE_END) {
		if (goldengate_cpu_freqs[i].index == cpu_info.freq) {
			pmode_cur = i;
			break;
		}
		i++;
	}

	if (goldengate_cpu_freqs[i].frequency == CPUFREQ_TABLE_END) {
		pr_err("unsupported frequency index: %d / %d\n",
		       cpu_info.freq, i);
		return -ENODEV;
	}

	pr_info("Registering CS75XX CPU frequency driver\n");
	pr_info("CPU Frequency: Low: %d MHz, High: %d MHz, Cur: %d MHz\n",
	       goldengate_cpu_freqs[0].frequency / 1000,
	       goldengate_cpu_freqs[max_index].frequency / 1000,
	       goldengate_cpu_freqs[pmode_cur].frequency / 1000);
	rc = cpufreq_register_driver(&goldengate_cpufreq_driver);

	return rc;
}

module_init(goldengate_cpufreq_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladimir Zapolskiy");
