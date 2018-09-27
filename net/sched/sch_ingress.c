/* net/sched/sch_ingress.c - Ingress qdisc
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Authors:     Jamal Hadi Salim 1999
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <net/netlink.h>
#include <net/pkt_sched.h>


struct ingress_qdisc_data {
	struct tcf_proto	*filter_list;
#ifdef CONFIG_CORTINA_GKCI
	u8 rate_enbl;
	u8 bypass_yellow;
	u8 bypass_red;
	u32 rate;	/* rate in kbps */
	u32 cbs;	/* commited burst size in bytes */
	u32 pbs;	/* peak burst size in bytes */
#endif
};

#ifdef CONFIG_CORTINA_GKCI
static const struct nla_policy ingress_policy[TCA_INGRESS_MAX + 1] = {
	[TCA_INGRESS_PARMS]	= { .len = sizeof(struct tc_ingress_qopt) },
};

void cs_qos_set_pol_cfg_ingress_qdisc(struct Qdisc *qdisc, u8 enbl, u8 bypass_yellow, u8 bypass_red, u32 rate_bps, u32 cbs, u32 pbs);
#endif

/* ------------------------- Class/flow operations ------------------------- */

static struct Qdisc *ingress_leaf(struct Qdisc *sch, unsigned long arg)
{
	return NULL;
}

static unsigned long ingress_get(struct Qdisc *sch, u32 classid)
{
	return TC_H_MIN(classid) + 1;
}

static unsigned long ingress_bind_filter(struct Qdisc *sch,
					 unsigned long parent, u32 classid)
{
	return ingress_get(sch, classid);
}

static void ingress_put(struct Qdisc *sch, unsigned long cl)
{
}

static void ingress_walk(struct Qdisc *sch, struct qdisc_walker *walker)
{
}

static struct tcf_proto **ingress_find_tcf(struct Qdisc *sch, unsigned long cl)
{
	struct ingress_qdisc_data *p = qdisc_priv(sch);

	return &p->filter_list;
}

/* --------------------------- Qdisc operations ---------------------------- */

static int ingress_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct ingress_qdisc_data *p = qdisc_priv(sch);
	struct tcf_result res;
	int result;

	result = tc_classify(skb, p->filter_list, &res);

	qdisc_bstats_update(sch, skb);
	switch (result) {
	case TC_ACT_SHOT:
		result = TC_ACT_SHOT;
		sch->qstats.drops++;
		break;
	case TC_ACT_STOLEN:
	case TC_ACT_QUEUED:
		result = TC_ACT_STOLEN;
		break;
	case TC_ACT_RECLASSIFY:
	case TC_ACT_OK:
		skb->tc_index = TC_H_MIN(res.classid);
	default:
		result = TC_ACT_OK;
		break;
	}

	return result;
}

/* ------------------------------------------------------------- */

static void ingress_destroy(struct Qdisc *sch)
{
	struct ingress_qdisc_data *p = qdisc_priv(sch);

	tcf_destroy_chain(&p->filter_list);
}

static int ingress_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	struct nlattr *nest;
#ifdef CONFIG_CORTINA_GKCI
	struct ingress_qdisc_data *q = qdisc_priv(sch);
	struct tc_ingress_qopt opt;
#endif

	nest = nla_nest_start(skb, TCA_OPTIONS);
	if (nest == NULL)
		goto nla_put_failure;
#ifdef CONFIG_CORTINA_GKCI
	opt.rate_enbl = q->rate_enbl;
	if (1 == q->rate_enbl) {
		memset(&opt.rate, 0, sizeof(opt.rate));
		opt.rate.rate = q->rate;
		opt.cbs = q->cbs;
		opt.pbs = q->pbs;
		opt.bypass_yellow = q->bypass_yellow;
		opt.bypass_red = q->bypass_red;
	}
	NLA_PUT(skb, TCA_INGRESS_PARMS, sizeof(opt), &opt);
#endif
	nla_nest_end(skb, nest);
	return skb->len;

nla_put_failure:
	nla_nest_cancel(skb, nest);
	return -1;
}

#ifdef CONFIG_CORTINA_GKCI
static int ingress_init(struct Qdisc *sch, struct nlattr *opt)
{
	struct ingress_qdisc_data *q = qdisc_priv(sch);
	int err;
	struct tc_ingress_qopt *qopt;
	struct nlattr *tb[TCA_INGRESS_MAX + 1];

	err = nla_parse_nested(tb, TCA_INGRESS_MAX, opt, ingress_policy);
	if (err < 0)
		return err;

	if (tb[TCA_INGRESS_PARMS] == NULL)
		return err;

	qopt = nla_data(tb[TCA_INGRESS_PARMS]);

	if ((qopt->rate_enbl == 0) || (qopt->rate_enbl == 1)) {
		/* only apply change when user attempts to enable/disable the rate 
		 * applied on top of ingress qdisc */
		if (qopt->rate_enbl == 0) {
			q->rate_enbl = 0;
		} else {
			if ((qopt->cbs < 128) || (qopt->cbs > 1048575)) {
				printk("Invalid cbs value. It must be with 128 to 1M bytes\n");
				return 0;
			}
			if ((qopt->pbs < 128) || (qopt->pbs > 1048575)) {
				printk("Invalid pbs value. It must be with 128 to 1M bytes\n");
				return 0;
			}
			if (((qopt->rate.rate / 1000) > (2621440 >> 3)) || 
					(((qopt->rate.rate / 1000) << 3) < 5)) {
				printk("Invalid rate value. It must be within 5kbp to 2.6 Gbps.\n");
				return 0;
			}
			q->rate_enbl = 1;
			q->rate = qopt->rate.rate;
			q->cbs = qopt->cbs;
			q->pbs = qopt->pbs;
			if (TCQ_INGRESS_NOT_SET8 != qopt->bypass_yellow)
				q->bypass_yellow = qopt->bypass_yellow;
			if (TCQ_INGRESS_NOT_SET8 != qopt->bypass_red)
				q->bypass_red = qopt->bypass_red;
		}
		cs_qos_set_pol_cfg_ingress_qdisc(sch, q->rate_enbl, 
				q->bypass_yellow, q->bypass_red, (q->rate << 3),
				q->cbs, q->pbs);
	} /* else, don't do any change to rate control */

	return 0;
}

static void ingress_reset(struct Qdisc *sch)
{
	struct ingress_qdisc_data *q = qdisc_priv(sch);

	/* FIXME! How about the policer that applies on top of those filters */

	/* when it's being reseted, make sure port policer is disabled. */
	q->rate_enbl = 0;
	q->bypass_yellow = 0;
	q->bypass_red = 0;
	q->rate = 0;
	q->cbs = 0;
	q->pbs = 0;
	cs_qos_set_pol_cfg_ingress_qdisc(sch, q->rate_enbl, q->bypass_yellow, 
			q->bypass_red, q->rate, q->cbs, q->pbs);
}
#endif

static const struct Qdisc_class_ops ingress_class_ops = {
	.leaf		=	ingress_leaf,
	.get		=	ingress_get,
	.put		=	ingress_put,
	.walk		=	ingress_walk,
	.tcf_chain	=	ingress_find_tcf,
	.bind_tcf	=	ingress_bind_filter,
	.unbind_tcf	=	ingress_put,
};

static struct Qdisc_ops ingress_qdisc_ops __read_mostly = {
	.cl_ops		=	&ingress_class_ops,
	.id		=	"ingress",
	.priv_size	=	sizeof(struct ingress_qdisc_data),
	.enqueue	=	ingress_enqueue,
	.destroy	=	ingress_destroy,
#ifdef CONFIG_CORTINA_GKCI
	.init		=	ingress_init,
	.change		=	ingress_init,
	.reset		=	ingress_reset,
#endif
	.dump		=	ingress_dump,
	.owner		=	THIS_MODULE,
};

static int __init ingress_module_init(void)
{
	return register_qdisc(&ingress_qdisc_ops);
}

static void __exit ingress_module_exit(void)
{
	unregister_qdisc(&ingress_qdisc_ops);
}

module_init(ingress_module_init)
module_exit(ingress_module_exit)
MODULE_LICENSE("GPL");
