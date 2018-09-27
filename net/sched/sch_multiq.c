/*
 * Copyright (c) 2008, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 *
 * Author: Alexander Duyck <alexander.h.duyck@intel.com>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <net/netlink.h>
#include <net/pkt_sched.h>


struct multiq_sched_data {
	u16 bands;
	u16 max_bands;
	u16 curband;
	struct tcf_proto *filter_list;
	struct Qdisc **queues;
#ifdef CONFIG_CS752X_HW_ACCELERATION
	u32 burst_size;
	u32 rate;	/* unit is byte/sec */
	u16 min_global_buffer;
	u16 max_global_buffer;
#endif
};

#ifdef CONFIG_CORTINA_GKCI
static u8 wred_mode = 2, wred_adj_range_idx = 0;

struct multisubq_sched_data {
	u8 band_id;
	u32 limit;
#ifdef CONFIG_CS752X_HW_ACCELERATION
	u32 rsrv_depth;
	u32 weight;
	u8 priority;
	u32 rate;
	struct tc_wredspec wred;
#endif
};

static const struct nla_policy multiq_policy[TCA_MULTIQ_MAX + 1] = {
	[TCA_MULTIQ_PARMS] = { .len = sizeof(struct tc_multiq_qopt) },
};

static const struct nla_policy multisubq_policy[TCA_MULTISUBQ_MAX + 1] = {
	[TCA_MULTISUBQ_PARMS] = { .len = sizeof(struct tc_multisubq_qopt) },
};

static struct Qdisc_ops multisubq_qdisc_ops;

extern void cs_qos_set_multiq_attribute(struct Qdisc *qdisc, u16 burst_size,
		u32 rate_bps, u16 min_global_buffer, u16 max_global_buffer,
		u8 wred_mode, u8 wred_adj_range_idx);
extern int cs_qos_set_multisubq_attribute(struct Qdisc *qdisc, u8 band_id,
		u8 priority, u32 weight, u32 rsrv_size, u32 max_size,
		u32 rate_bps, void *p_wred);
extern int cs_qos_get_multisubq_depth(struct Qdisc *qdisc, u8 band_id,
		u16 *p_min_depth, u16 *p_max_depth);
extern void cs_qos_reset_multiq(struct Qdisc *qdisc);
extern void cs_qos_reset_multisubq(struct Qdisc *qdisc, u8 band_id);

#ifdef CONFIG_CS752X_HW_ACCELERATION
extern int cs_qos_check_and_steal_multiq_skb(struct Qdisc *qdisc,
		struct sk_buff *skb);
#endif
#endif /* CONFIG_CORTINA_GKCI */

static struct Qdisc *
multiq_classify(struct sk_buff *skb, struct Qdisc *sch, int *qerr)
{
	struct multiq_sched_data *q = qdisc_priv(sch);
	u32 band;
	struct tcf_result res;
	int err;

	*qerr = NET_XMIT_SUCCESS | __NET_XMIT_BYPASS;
	err = tc_classify(skb, q->filter_list, &res);
#ifdef CONFIG_NET_CLS_ACT
	switch (err) {
	case TC_ACT_STOLEN:
	case TC_ACT_QUEUED:
		*qerr = NET_XMIT_SUCCESS | __NET_XMIT_STOLEN;
	case TC_ACT_SHOT:
		return NULL;
	}
#endif
	band = skb_get_queue_mapping(skb);

	if (band >= q->bands)
		return q->queues[0];

	return q->queues[band];
}

static int
multiq_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct Qdisc *qdisc;
	int ret;
#ifdef CONFIG_CORTINA_GKCI
	unsigned pkt_len;
#endif

	qdisc = multiq_classify(skb, sch, &ret);
#ifdef CONFIG_NET_CLS_ACT
	if (qdisc == NULL) {

		if (ret & __NET_XMIT_BYPASS)
			sch->qstats.drops++;
		kfree_skb(skb);
		return ret;
	}
#endif

#ifdef CONFIG_CS752X_HW_ACCELERATION
	/* Cortina Acceleration
	 * see if this packet is enqueuing into a MultiQ mapped to a HW queue,
	 * if so, hijack the packet, just transmit it */
	pkt_len = qdisc_pkt_len(skb);
	if (NET_XMIT_SUCCESS ==
			(cs_qos_check_and_steal_multiq_skb)(qdisc, skb)) {
		sch->bstats.bytes += pkt_len;
		sch->bstats.packets++;
		return NET_XMIT_SUCCESS;
	}
#endif
	ret = qdisc_enqueue(skb, qdisc);
	if (ret == NET_XMIT_SUCCESS) {
#ifdef CONFIG_CORTINA_GKCI
		sch->bstats.bytes += qdisc_pkt_len(skb);
		sch->bstats.packets++;
#endif
		sch->q.qlen++;
		return NET_XMIT_SUCCESS;
	}
	if (net_xmit_drop_count(ret))
		sch->qstats.drops++;
	return ret;
}
#ifdef CONFIG_CORTINA_GKCI
static int
multisubq_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct multisubq_sched_data *q = qdisc_priv(sch);

	/* multisubq_enqueue is following pfifo_enqueue() as the default
	 * enqueue method. It will not be modified.  Rate and other attributes
	 * are directly applied on top of hardware queue, so they are not visible
	 * by software. */
	if (likely(sch->qstats.backlog + qdisc_pkt_len(skb) <= q->limit))
		return qdisc_enqueue_tail(skb, sch);

	return qdisc_reshape_fail(skb, sch);
}
#endif

static struct sk_buff *multiq_dequeue(struct Qdisc *sch)
{
	struct multiq_sched_data *q = qdisc_priv(sch);
	struct Qdisc *qdisc;
	struct sk_buff *skb;
	int band;

	for (band = 0; band < q->bands; band++) {
		/* cycle through bands to ensure fairness */
		q->curband++;
		if (q->curband >= q->bands)
			q->curband = 0;

		/* Check that target subqueue is available before
		 * pulling an skb to avoid head-of-line blocking.
		 */
		if (!netif_xmit_stopped(
		    netdev_get_tx_queue(qdisc_dev(sch), q->curband))) {
			qdisc = q->queues[q->curband];
			skb = qdisc->dequeue(qdisc);
			if (skb) {
				qdisc_bstats_update(sch, skb);
				sch->q.qlen--;
				return skb;
			}
		}
	}
	return NULL;

}

static struct sk_buff *multiq_peek(struct Qdisc *sch)
{
	struct multiq_sched_data *q = qdisc_priv(sch);
	unsigned int curband = q->curband;
	struct Qdisc *qdisc;
	struct sk_buff *skb;
	int band;

	for (band = 0; band < q->bands; band++) {
		/* cycle through bands to ensure fairness */
		curband++;
		if (curband >= q->bands)
			curband = 0;

		/* Check that target subqueue is available before
		 * pulling an skb to avoid head-of-line blocking.
		 */
		if (!netif_xmit_stopped(
		    netdev_get_tx_queue(qdisc_dev(sch), curband))) {
			qdisc = q->queues[curband];
			skb = qdisc->ops->peek(qdisc);
			if (skb)
				return skb;
		}
	}
	return NULL;

}

static unsigned int multiq_drop(struct Qdisc *sch)
{
	struct multiq_sched_data *q = qdisc_priv(sch);
	int band;
	unsigned int len;
	struct Qdisc *qdisc;

	for (band = q->bands - 1; band >= 0; band--) {
		qdisc = q->queues[band];
		if (qdisc->ops->drop) {
			len = qdisc->ops->drop(qdisc);
			if (len != 0) {
				sch->q.qlen--;
				return len;
			}
		}
	}
	return 0;
}


static void
multiq_reset(struct Qdisc *sch)
{
	u16 band;
	struct multiq_sched_data *q = qdisc_priv(sch);

	for (band = 0; band < q->bands; band++)
		qdisc_reset(q->queues[band]);
	sch->q.qlen = 0;
	q->curband = 0;
}

static void
multiq_destroy(struct Qdisc *sch)
{
	int band;
	struct multiq_sched_data *q = qdisc_priv(sch);

	tcf_destroy_chain(&q->filter_list);
	for (band = 0; band < q->bands; band++)
		qdisc_destroy(q->queues[band]);

#ifdef CONFIG_CS752X_HW_ACCELERATION
	cs_qos_reset_multiq(sch);
#endif
	kfree(q->queues);
}

#ifdef CONFIG_CORTINA_GKCI

static void
multisubq_destroy(struct Qdisc *sch)
{
#ifdef CONFIG_CS752X_HW_ACCELERATION
	struct multisubq_sched_data *q = qdisc_priv(sch);

	cs_qos_reset_multisubq(sch, q->band_id);
#endif
	return;
}

static void qdisc_list_add2(struct Qdisc *q, struct Qdisc *parent)
{
	list_add_tail(&q->list, &parent->list);
}

#endif

static int multiq_tune(struct Qdisc *sch, struct nlattr *opt)
{
	struct multiq_sched_data *q = qdisc_priv(sch);
	struct tc_multiq_qopt *qopt;
	int i;
#ifdef CONFIG_CORTINA_GKCI
	int err;
	struct nlattr *tb[TCA_MULTIQ_MAX + 1];
	struct Qdisc *child, *old;
	struct multisubq_sched_data *child_data;
#endif

	if (!netif_is_multiqueue(qdisc_dev(sch)))
		return -EOPNOTSUPP;
#ifdef CONFIG_CORTINA_GKCI
#if 0	// FIXME!! do we need the following?
	if (nla_len(opt) < sizeof(*qopt))
		return -EINVAL;
#endif
#else
	if (nla_len(opt) < sizeof(*qopt))
		return -EINVAL;

	qopt = nla_data(opt);

	qopt->bands = qdisc_dev(sch)->real_num_tx_queues;

	sch_tree_lock(sch);
	q->bands = qopt->bands;
#endif
#if defined(CONFIG_CORTINA_GKCI) && defined(CONFIG_CS752X_HW_ACCELERATION)
	err = nla_parse_nested(tb, TCA_MULTIQ_MAX, opt, multiq_policy);
	if (err < 0)
		return err;

	if (tb[TCA_MULTIQ_PARMS] == NULL)
		return err;

	qopt = nla_data(tb[TCA_MULTIQ_PARMS]);

	if ((0 != qopt->bands) && (qopt->bands <= q->max_bands))
		q->bands = qopt->bands;


	if (qopt->rate.rate != TCQ_MULTIQ_NOT_SET32)
		q->rate = qopt->rate.rate;
	if (qopt->burst_size != TCQ_MULTIQ_NOT_SET32)
		q->burst_size = qopt->burst_size;
	if (qopt->min_global_buffer != TCQ_MULTIQ_NOT_SET16)
		q->min_global_buffer = qopt->min_global_buffer;
	if (qopt->max_global_buffer != TCQ_MULTIQ_NOT_SET16)
		q->max_global_buffer = qopt->max_global_buffer;
	if (qopt->wred_mode != TCQ_MULTIQ_NOT_SET8)
		wred_mode = qopt->wred_mode;
	if (qopt->wred_adj_range_idx != TCQ_MULTIQ_NOT_SET8)
		wred_adj_range_idx = qopt->wred_adj_range_idx;

	cs_qos_set_multiq_attribute(sch, q->burst_size, (q->rate << 3),
			q->min_global_buffer, q->max_global_buffer,
			wred_mode, wred_adj_range_idx);

	sch_tree_lock(sch);
#endif /* CORTINA_GKCI */


	for (i = q->bands; i < q->max_bands; i++) {
		if (q->queues[i] != &noop_qdisc) {
			struct Qdisc *child = q->queues[i];
			q->queues[i] = &noop_qdisc;
			qdisc_tree_decrease_qlen(child, child->q.qlen);
			qdisc_destroy(child);
		}
	}

	sch_tree_unlock(sch);

#if defined(CONFIG_CORTINA_GKCI) && defined(CONFIG_CS752X_HW_ACCELERATION)
	 for (i = 0; i < q->bands; i++) {
		if (q->queues[i] == &noop_qdisc) {
			struct Qdisc *child, *old;
			child = qdisc_create_dflt(sch->dev_queue,
						  &multisubq_qdisc_ops,
						  sch->handle);
						  //TC_H_MAKE(sch->handle, i + 1));
			if (child) {
				child->handle = TC_H_MAKE(TC_H_MAJ(sch->handle) * 16 + (i << 16), 0);
				sch_tree_lock(sch);
				old = q->queues[i];
				q->queues[i] = child;
				child_data = qdisc_priv(child);
				child_data->band_id = i;

				if (old != &noop_qdisc) {
					qdisc_tree_decrease_qlen(old,
								 old->q.qlen);
					qdisc_destroy(old);
				}
				sch_tree_unlock(sch);
				qdisc_list_add2(child, sch);
				/* Cortina Acceleration
				 * Mark the MultiQ in Qdisc, because this qdisc is 1:1 mapped to
				 * HW queue. */
				q->queues[i]->cs_handle |= CS_QOS_HWQ_MAP + i;
			}
		}
	}
#else /* CORTINA_GKCI */
    for (i = 0; i < q->bands; i++) {
		if (q->queues[i] == &noop_qdisc) {
			struct Qdisc *child, *old;
			child = qdisc_create_dflt(sch->dev_queue,
						  &pfifo_qdisc_ops,
						  TC_H_MAKE(sch->handle,
							    i + 1));
			if (child) {
				sch_tree_lock(sch);
				old = q->queues[i];
				q->queues[i] = child;

				if (old != &noop_qdisc) {
					qdisc_tree_decrease_qlen(old,
								 old->q.qlen);
					qdisc_destroy(old);
				}
				sch_tree_unlock(sch);
			}
		}
	}

#endif /* CORTINA_GKCI */
	return 0;
}

static int multiq_init(struct Qdisc *sch, struct nlattr *opt)
{
	struct multiq_sched_data *q = qdisc_priv(sch);
	int i, err;

	q->queues = NULL;

	if (opt == NULL)
		return -EINVAL;

	q->max_bands = qdisc_dev(sch)->num_tx_queues;


#ifdef CONFIG_CS752X_HW_ACCELERATION
	q->bands = q->max_bands;
	q->burst_size = 0;
	q->rate = 0;
	q->min_global_buffer = 0;
	q->max_global_buffer = 0;

	/* Cortina Acceleration
	 * Mark the MultiQ in Qdisc, so we can tell the type of qdisc later on */
	sch->cs_handle = CS_QOS_IS_MULTIQ;

	q->queues = kcalloc(q->max_bands, sizeof(struct Qdisc *), GFP_KERNEL);
	if (!q->queues)
		return -ENOBUFS;
	for (i = 0; i < q->max_bands; i++) {
		q->queues[i] = &noop_qdisc;
		/* Cortina Acceleration
		 * Mark the MultiQ in Qdisc, so we can tell the type of qdisc
		 * later on */
		q->queues[i]->cs_handle = CS_QOS_IS_MULTIQ;
	}
#else /* CS752x_HW_ACCELERATION */
	q->queues = kcalloc(q->max_bands, sizeof(struct Qdisc *), GFP_KERNEL);
	if (!q->queues)
		return -ENOBUFS;
	for (i = 0; i < q->max_bands; i++)
		q->queues[i] = &noop_qdisc;

#endif /* CS752x_HW_ACCELERATION */
	err = multiq_tune(sch,opt);

	if (err)
		kfree(q->queues);

	return err;
}

#if defined(CONFIG_CORTINA_GKCI) && defined(CONFIG_CS752X_HW_ACCELERATION)
static int multisubq_init(struct Qdisc *sch, struct nlattr *opt)
{
	struct multisubq_sched_data *q = qdisc_priv(sch);

	if (opt == NULL) {
		u16 limit, rsrv_depth;

		/* at this point, band_id is always 0, but we are retrieving
		 * initial value from HW table. All other bands should have the
		 * same initialized value */
		cs_qos_get_multisubq_depth(sch, q->band_id,
				&rsrv_depth, &limit);
		q->rsrv_depth = (u32)rsrv_depth;
		q->limit = (u32)limit;
#define CS_SCHED_DEFAULT_WEIGHT		9132
		q->weight = CS_SCHED_DEFAULT_WEIGHT;
		q->priority = 0;
		q->rate = 0;
		memset(&q->wred, 0, sizeof(struct tc_wredspec));
		q->wred.min_pct_buffer = TCQ_MULTIQ_NOT_SET8;
		q->wred.max_pct_buffer = TCQ_MULTIQ_NOT_SET8;
		/* at this point, we don't know band_id yet, so, ignore the following */
#if 0
		cs_qos_set_multisubq_attribute(sch, q->band_id, q->priority,
				q->weight, q->rsrv_depth, q->limit, q->rate, NULL);
#endif
	} else {
		int err;
		struct nlattr *tb[TCA_MULTISUBQ_MAX + 1];
		struct tc_multisubq_qopt *qopt;
		u32 new_max;
#ifdef CONFIG_CS752X_HW_ACCELERATION
		struct tc_wredspec new_wred, *p_wred = NULL;
		u8 new_prio;
		u32 new_weight, new_min, new_rate;
#endif

		err = nla_parse_nested(tb, TCA_MULTISUBQ_MAX, opt, multisubq_policy);
		if (err < 0)
			return err;

		err = -EINVAL;
		if (tb[TCA_MULTISUBQ_PARMS] == NULL)
			goto done;

		qopt = nla_data(tb[TCA_MULTISUBQ_PARMS]);


		if (TCQ_MULTIQ_NOT_SET32 != qopt->limit)
			new_max = qopt->limit;
		else new_max = q->limit;

#ifdef CONFIG_CS752X_HW_ACCELERATION
		if (qopt->rsrv_depth != TCQ_MULTIQ_NOT_SET32)
			new_min = qopt->rsrv_depth;
		else new_min = q->rsrv_depth;
		if (qopt->weight != TCQ_MULTIQ_NOT_SET32) {
			new_weight = qopt->weight;
			new_prio = 0;
		} else if (qopt->priority != TCQ_MULTIQ_NOT_SET8) {
			new_weight = 0;
			new_prio = qopt->priority;
		} else {
			new_weight = q->weight;
			new_prio = q->priority;
		}
		if (qopt->rate.rate != TCQ_MULTIQ_NOT_SET32)
			new_rate = qopt->rate.rate;
		else new_rate = q->rate;

		if (qopt->wred.enbl != TCQ_MULTIQ_NOT_SET8) {
			memcpy(&new_wred, &qopt->wred, sizeof(struct tc_wredspec));
			p_wred = &new_wred;
			if (p_wred->min_pct_buffer == TCQ_MULTIQ_NOT_SET8)
				p_wred->min_pct_buffer = q->wred.min_pct_buffer;
			if (p_wred->max_pct_buffer == TCQ_MULTIQ_NOT_SET8)
				p_wred->max_pct_buffer = q->wred.max_pct_buffer;
			if (p_wred->aqd_lp_filter_const == TCQ_MULTIQ_NOT_SET8)
				p_wred->aqd_lp_filter_const = q->wred.aqd_lp_filter_const;
		}

		/* first set up the value to HW. if fails, then don't have to update
		 * SW values */
		err = cs_qos_set_multisubq_attribute(sch, q->band_id,
					new_prio, new_weight, new_min, new_max,
					(new_rate << 3), (void *)p_wred);
		if (err < 0) {
			printk("Inappropriate values setup for multisubq\n");
			return err;
		}
#endif

		q->limit = new_max;
#ifdef CONFIG_CS752X_HW_ACCELERATION
		q->rsrv_depth = new_min;
		q->weight = new_weight;
		q->priority = new_prio;
		q->rate = new_rate;

		if (qopt->wred.enbl != TCQ_MULTIQ_NOT_SET8) {
			p_wred = &q->wred;
			if (qopt->wred.enbl == TRUE) {
				p_wred->enbl = TRUE;
				p_wred->min_pct_base = qopt->wred.min_pct_base;
				p_wred->max_pct_base = qopt->wred.max_pct_base;
				p_wred->drop_prob = qopt->wred.drop_prob;
				if ((qopt->wred.min_pct_buffer != TCQ_MULTIQ_NOT_SET8) &&
						(qopt->wred.max_pct_buffer != TCQ_MULTIQ_NOT_SET8)) {
					p_wred->min_pct_buffer = qopt->wred.min_pct_buffer;
					p_wred->max_pct_buffer = qopt->wred.max_pct_buffer;
				}
				if (qopt->wred.aqd_lp_filter_const != TCQ_MULTIQ_NOT_SET8)
					p_wred->aqd_lp_filter_const = qopt->wred.aqd_lp_filter_const;
			} else {
				memset(p_wred, 0, sizeof(struct tc_wredspec));
				p_wred->enbl = FALSE;
			}
		}

#endif
		err = 0;
done:
		return err;
	}

	return 0;
}


static int multiq_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	struct multiq_sched_data *q = qdisc_priv(sch);
	//unsigned char *b = skb_tail_pointer(skb);
	struct tc_multiq_qopt opt;
	struct nlattr *nest;

	nest = nla_nest_start(skb, TCA_OPTIONS);
	if (nest == NULL)
		goto nla_put_failure;

	opt.bands = q->bands;
	opt.max_bands = q->max_bands;
#ifdef CONFIG_CS752X_HW_ACCELERATION
	opt.burst_size = q->burst_size;
	opt.min_global_buffer = q->min_global_buffer;
	opt.max_global_buffer = q->max_global_buffer;
	opt.wred_mode = wred_mode;
	opt.wred_adj_range_idx = wred_adj_range_idx;
	memset(&opt.rate, 0, sizeof(opt.rate));
	if (q->rate != 0)
	opt.rate.rate = q->rate;
#endif
	NLA_PUT(skb, TCA_MULTIQ_PARMS, sizeof(opt), &opt);

	nla_nest_end(skb, nest);
	return skb->len;

nla_put_failure:
	nla_nest_cancel(skb, nest);
	return -1;
}


static int multisubq_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	struct multisubq_sched_data *q = qdisc_priv(sch);
	struct tc_multisubq_qopt opt;
	struct nlattr *nest;

	nest = nla_nest_start(skb, TCA_OPTIONS);
	if (nest == NULL)
		goto nla_put_failure;

	opt.limit = q->limit;
#ifdef CONFIG_CS752X_HW_ACCELERATION
	opt.rsrv_depth = q->rsrv_depth;
	opt.weight = q->weight;
	opt.priority = q->priority;
	memset(&opt.rate, 0, sizeof(opt.rate));
	if (q->rate != 0)
	opt.rate.rate = q->rate;
	if (q->wred.enbl == TRUE) {
		memcpy(&opt.wred, &q->wred, sizeof(struct tc_wredspec));
	} else {
		opt.wred.enbl = FALSE;
	}
#endif

	NLA_PUT(skb, TCA_MULTISUBQ_PARMS, sizeof(opt), &opt);

	nla_nest_end(skb, nest);
	return skb->len;

nla_put_failure:
	nla_nest_cancel(skb, nest);
	return -1;
}
#else /* CONFIG_CORTINA_GKCI */
static int multiq_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	struct multiq_sched_data *q = qdisc_priv(sch);
	unsigned char *b = skb_tail_pointer(skb);
	struct tc_multiq_qopt opt;

	opt.bands = q->bands;
	opt.max_bands = q->max_bands;

	NLA_PUT(skb, TCA_OPTIONS, sizeof(opt), &opt);

	return skb->len;

nla_put_failure:
	nlmsg_trim(skb, b);
	return -1;
}
#endif /* CONFIG_CORTINA_GKCI */
static int multiq_graft(struct Qdisc *sch, unsigned long arg, struct Qdisc *new,
		      struct Qdisc **old)
{
	struct multiq_sched_data *q = qdisc_priv(sch);
	unsigned long band = arg - 1;

	if (new == NULL)
		new = &noop_qdisc;

	sch_tree_lock(sch);
	*old = q->queues[band];
	q->queues[band] = new;
	qdisc_tree_decrease_qlen(*old, (*old)->q.qlen);
	qdisc_reset(*old);
	sch_tree_unlock(sch);

	return 0;
}

static struct Qdisc *
multiq_leaf(struct Qdisc *sch, unsigned long arg)
{
	struct multiq_sched_data *q = qdisc_priv(sch);
	unsigned long band = arg - 1;

	return q->queues[band];
}

static unsigned long multiq_get(struct Qdisc *sch, u32 classid)
{
	struct multiq_sched_data *q = qdisc_priv(sch);
	unsigned long band = TC_H_MIN(classid);

	if (band - 1 >= q->bands)
		return 0;
	return band;
}

static unsigned long multiq_bind(struct Qdisc *sch, unsigned long parent,
				 u32 classid)
{
	return multiq_get(sch, classid);
}


static void multiq_put(struct Qdisc *q, unsigned long cl)
{
}

static int multiq_dump_class(struct Qdisc *sch, unsigned long cl,
			     struct sk_buff *skb, struct tcmsg *tcm)
{
	struct multiq_sched_data *q = qdisc_priv(sch);

	tcm->tcm_handle |= TC_H_MIN(cl);
	tcm->tcm_info = q->queues[cl - 1]->handle;
	return 0;
}

static int multiq_dump_class_stats(struct Qdisc *sch, unsigned long cl,
				 struct gnet_dump *d)
{
	struct multiq_sched_data *q = qdisc_priv(sch);
	struct Qdisc *cl_q;

	cl_q = q->queues[cl - 1];
	cl_q->qstats.qlen = cl_q->q.qlen;
	if (gnet_stats_copy_basic(d, &cl_q->bstats) < 0 ||
	    gnet_stats_copy_queue(d, &cl_q->qstats) < 0)
		return -1;

	return 0;
}

static void multiq_walk(struct Qdisc *sch, struct qdisc_walker *arg)
{
	struct multiq_sched_data *q = qdisc_priv(sch);
	int band;

	if (arg->stop)
		return;

	for (band = 0; band < q->bands; band++) {
		if (arg->count < arg->skip) {
			arg->count++;
			continue;
		}
		if (arg->fn(sch, band + 1, arg) < 0) {
			arg->stop = 1;
			break;
		}
		arg->count++;
	}
}

static struct tcf_proto **multiq_find_tcf(struct Qdisc *sch, unsigned long cl)
{
	struct multiq_sched_data *q = qdisc_priv(sch);

	if (cl)
		return NULL;
	return &q->filter_list;
}

static const struct Qdisc_class_ops multiq_class_ops = {
	.graft		=	multiq_graft,
	.leaf		=	multiq_leaf,
	.get		=	multiq_get,
	.put		=	multiq_put,
	.walk		=	multiq_walk,
	.tcf_chain	=	multiq_find_tcf,
	.bind_tcf	=	multiq_bind,
	.unbind_tcf	=	multiq_put,
	.dump		=	multiq_dump_class,
	.dump_stats	=	multiq_dump_class_stats,
};

static struct Qdisc_ops multiq_qdisc_ops __read_mostly = {
	.next		=	NULL,
	.cl_ops		=	&multiq_class_ops,
	.id		=	"multiq",
	.priv_size	=	sizeof(struct multiq_sched_data),
	.enqueue	=	multiq_enqueue,
	.dequeue	=	multiq_dequeue,
	.peek		=	multiq_peek,
	.drop		=	multiq_drop,
	.init		=	multiq_init,
	.reset		=	multiq_reset,
	.destroy	=	multiq_destroy,
	.change		=	multiq_tune,
	.dump		=	multiq_dump,
	.owner		=	THIS_MODULE,
};

#if defined(CONFIG_CORTINA_GKCI) && defined(CONFIG_CS752X_HW_ACCELERATION)

static struct Qdisc_ops multisubq_qdisc_ops __read_mostly = {
	.id			=	"multisubq",
	.priv_size	=	sizeof(struct multisubq_sched_data),
	.enqueue	=	multisubq_enqueue,
	.dequeue	=	qdisc_dequeue_head,
	.peek		=	qdisc_peek_head,
	.drop		=	qdisc_queue_drop,
	.init		=	multisubq_init,
	.reset		=	qdisc_reset_queue,
	.destroy	=	multisubq_destroy,
	.change		=	multisubq_init,
	.dump		=	multisubq_dump,
	.owner		=	THIS_MODULE,
};

#endif

static int __init multiq_module_init(void)
{
#ifdef CONFIG_CORTINA_GKCI
	int rc = register_qdisc(&multisubq_qdisc_ops);
	if (rc)
		return rc;
#endif
	return register_qdisc(&multiq_qdisc_ops);
}

static void __exit multiq_module_exit(void)
{
#ifdef CONFIG_CORTINA_GKCI
	unregister_qdisc(&multisubq_qdisc_ops);
#endif
	unregister_qdisc(&multiq_qdisc_ops);
}

module_init(multiq_module_init)
module_exit(multiq_module_exit)

MODULE_LICENSE("GPL");
