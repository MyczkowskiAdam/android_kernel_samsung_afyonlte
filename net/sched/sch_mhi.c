/*
 * Copyright (c) 2011, Renesas Mobile Corporation.
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
 * Author: Petri Mattila <petri.to.mattila@renesasmobile.com>
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <linux/notifier.h>
#include <linux/netdevice.h>
#include <net/netlink.h>
#include <net/pkt_sched.h>
#include <net/sch_generic.h>
#include <net/mhi/sched.h>


#define USE_DEBUG

#ifdef USE_DEBUG
# define DPRINTK(...)    printk(KERN_DEBUG "SCH_MHI: " __VA_ARGS__)
# define BUGGER(cond)    BUG_ON(cond)
#else
# define DPRINTK(...)
# define BUGGER(cond)
#endif


/*** Local types ***/

struct mhi_qdisc {
	/* Number of hardware queues */
	int nq;

	/* Preallocated Qdiscs */
	struct Qdisc **qdiscs;
};

struct mhdp_qdisc {
	int state;
	int limit;
	int lowmark;
	int highmark;
	struct srcu_notifier_head qnot;
};

#define QUEUE_LOW   0
#define QUEUE_HIGH  1


/*** Forward declarations ***/

static struct Qdisc_ops *mhi_queue_ops[];

static struct Qdisc_ops mhi_qdisc_ops;
static struct Qdisc_ops mhi_mhdp_ops;
static struct Qdisc_ops mhi_fifo_ops;
static struct Qdisc_ops mhi_noq_ops;

static int mhi_qdisc_tab[];


/*** NULL Qdisc operations ***/

static struct sk_buff *null_dequeue(struct Qdisc * sch)
{
	return NULL;
}


/*** MHDP Qdisc operations ***/

static int mhdp_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct mhdp_qdisc *priv = qdisc_priv(sch);
	int ret = 0;

	DPRINTK("mhdp_enqueue: SKB len:%d mapping:%d",
			skb->len,
			skb->queue_mapping);

	qdisc_enqueue_tail(skb, sch);

	if (priv->state == QUEUE_LOW && qdisc_qlen(sch) >= priv->highmark) {
		DPRINTK("mhdp_enqnueue: NOTIFY QUEUE HIGH LEVEL");
		srcu_notifier_call_chain(&priv->qnot,
					 MHI_NOTIFY_QUEUE_HIGH,
					 qdisc_dev(sch));
		priv->state = QUEUE_HIGH;
	}

	return ret;
}

static struct sk_buff *mhdp_dequeue(struct Qdisc *sch)
{
	struct mhdp_qdisc *priv = qdisc_priv(sch);
	struct sk_buff *skb;

	skb = qdisc_dequeue_head(sch);

	if (skb) {
		if (priv->state == QUEUE_HIGH && qdisc_qlen(sch) <= priv->lowmark) {
			DPRINTK("mhdp_enqnueue: NOTIFY QUEUE LOW LEVEL");
			srcu_notifier_call_chain(&priv->qnot,
						 MHI_NOTIFY_QUEUE_LOW,
						 qdisc_dev(sch));
			priv->state = QUEUE_LOW;
		}

		DPRINTK("mhdp_dequeue: SKB len:%d mapping:%d",
				skb->len,
				skb->queue_mapping);
	}

	return skb;

}

static unsigned int mhdp_drop(struct Qdisc *sch)
{
	struct mhdp_qdisc *priv = qdisc_priv(sch);
	unsigned int len = 0;

	len = qdisc_queue_drop_head(sch);

	if (len) {
		if (priv->state == QUEUE_HIGH &&
			qdisc_qlen(sch) <= priv->lowmark) {
				DPRINTK("mhdp_enqnueue: NOTIFY QUEUE LOW LEVEL");
				srcu_notifier_call_chain(&priv->qnot,
						 MHI_NOTIFY_QUEUE_LOW,
						 qdisc_dev(sch));
				priv->state = QUEUE_LOW;
		}
	}

	DPRINTK("mhi_drop: len:%d", len);

	return len;
}

static int mhdp_tune(struct Qdisc *sch, struct nlattr *opt)
{
	struct mhdp_qdisc *priv = qdisc_priv(sch);
	struct tc_mhdp_qopt *qopt;

	DPRINTK("mhdp_tune");

	if (!opt)
		return -EINVAL;

	if (nla_len(opt) < sizeof(*qopt))
		return -EINVAL;

	qopt = nla_data(opt);

	if (qopt->limit < 1)
		qopt->limit = 1000;

	if (qopt->limit > 1000)
		qopt->limit = 1000;

	if (qopt->highmark < 2)
		qopt->highmark = 2;

	if (qopt->highmark > qopt->limit - 1)
		qopt->highmark = qopt->limit - 1;

	if (qopt->lowmark < 1)
		qopt->lowmark = 1;

	if (qopt->lowmark > qopt->highmark - 1)
		qopt->lowmark = qopt->highmark - 1;

	priv->limit    = qopt->limit;
	priv->lowmark  = qopt->lowmark;
	priv->highmark = qopt->highmark;

	DPRINTK("mhdp_tune: limit:%d lowmark:%d highmark:%d",
		priv->limit, priv->lowmark,  priv->highmark);

	return 0;
}

static int mhdp_init(struct Qdisc *sch, struct nlattr *opt)
{
	struct mhdp_qdisc *priv = qdisc_priv(sch);
	int err = 0;

	DPRINTK("mhdp_init");

	priv->state    = QUEUE_LOW;
	priv->limit    = 1000;
	priv->lowmark  = 1;
	priv->highmark = 3;

	srcu_init_notifier_head(&priv->qnot);

	if (opt)
		err = mhdp_tune(sch, opt);

	DPRINTK("mhdp_init: %s (%d)", err ? "ERROR" : "OK", err);

	return err;
}

static void mhdp_destroy(struct Qdisc *sch)
{
	struct mhdp_qdisc *priv = qdisc_priv(sch);

	DPRINTK("mhdp_destroy");

	srcu_cleanup_notifier_head(&priv->qnot);
}


/*** MHI Qdisc operations ***/

static struct Qdisc_ops *mhi_get_qdisc_ops(int queue)
{
	int qcl;

	if (queue < 0 || queue > 7)
		qcl = 1;
	else
		qcl = mhi_qdisc_tab[queue];

	if (qcl < 0 || qcl > 2)
		qcl = 1;

	return mhi_queue_ops[qcl];
}


static void mhi_destroy(struct Qdisc *sch)
{
	struct mhi_qdisc *priv = qdisc_priv(sch);
	int i;

	DPRINTK("mhi_destroy");

	if (priv->qdiscs) {
		for (i = 0; i < priv->nq; i++)
			if (priv->qdiscs[i])
				qdisc_destroy(priv->qdiscs[i]);
		kfree(priv->qdiscs);
	}
}

static int mhi_init(struct Qdisc *sch, struct nlattr *opt)
{
	struct mhi_qdisc *priv = qdisc_priv(sch);
	struct net_device *dev = qdisc_dev(sch);
	struct netdev_queue *qdev;
	struct Qdisc_ops *qops;
	unsigned int i;

	DPRINTK("mhi_init");

	if (sch->parent != TC_H_ROOT)
		return -EOPNOTSUPP;

	if (!netif_is_multiqueue(dev))
		return -EOPNOTSUPP;

	priv->nq = dev->num_tx_queues;

	priv->qdiscs = kcalloc(priv->nq, sizeof(struct Qdisc *), GFP_KERNEL);
	if (!priv->qdiscs)
		goto rollback0;

	for (i = 0; i < priv->nq; i++) {
		qdev = netdev_get_tx_queue(dev, i);
		qops = mhi_get_qdisc_ops(i);

		priv->qdiscs[i] =
			qdisc_create_dflt(/*dev,*/ qdev, qops,
				TC_H_MAKE(TC_H_MAJ(sch->handle), TC_H_MIN(i+1)));/*PATCH GBR : dev no more used*/

		if (!priv->qdiscs[i])
			goto rollback1;

		try_module_get(qops->owner); /* released in qdisc_destroy() */
	}

	sch->flags |= TCQ_F_MQROOT;

	return 0;

rollback1:
	mhi_destroy(sch);
rollback0:
	return -ENOMEM;
}

static void mhi_attach(struct Qdisc *sch)
{
	struct mhi_qdisc *priv = qdisc_priv(sch);
	struct Qdisc *qdisc;
	int i;

	DPRINTK("mhi_attach");
	BUGGER(priv->qdiscs == NULL);

	for (i = 0; i < priv->nq; i++) {
		qdisc = dev_graft_qdisc(priv->qdiscs[i]->dev_queue,
					priv->qdiscs[i]);
		if (qdisc)
			qdisc_destroy(qdisc);
	}

	kfree(priv->qdiscs);
	priv->qdiscs = NULL;
}

static int mhi_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	struct mhi_qdisc *priv = qdisc_priv(sch);
	struct net_device *dev = qdisc_dev(sch);
	struct netdev_queue *qdev;
	struct Qdisc *qdisc;
	int i;

	DPRINTK("mhi_dump");

	sch->q.qlen = 0;

	memset(&sch->bstats, 0, sizeof(sch->bstats));
	memset(&sch->qstats, 0, sizeof(sch->qstats));

	for (i = 0; i < priv->nq; i++) {
		qdev = netdev_get_tx_queue(dev, i);
		BUGGER(qdev == NULL);

		qdisc = qdev->qdisc_sleeping;

		spin_lock_bh(qdisc_lock(qdisc));
		{
			sch->q.qlen		+= qdisc->q.qlen;
			sch->bstats.bytes	+= qdisc->bstats.bytes;
			sch->bstats.packets	+= qdisc->bstats.packets;
			sch->qstats.qlen	+= qdisc->qstats.qlen;
			sch->qstats.backlog	+= qdisc->qstats.backlog;
			sch->qstats.drops	+= qdisc->qstats.drops;
			sch->qstats.requeues	+= qdisc->qstats.requeues;
			sch->qstats.overlimits	+= qdisc->qstats.overlimits;
		}
		spin_unlock_bh(qdisc_lock(qdisc));
	}
	return 0;
}

static struct netdev_queue *mhi_netdev_queue_get(struct Qdisc *sch,
							unsigned long cl)
{
	struct net_device *dev = qdisc_dev(sch);

	if (cl < 1 || cl > dev->num_tx_queues)
		return NULL;

	return netdev_get_tx_queue(dev, cl-1);
}

static struct netdev_queue *mhi_select_queue(struct Qdisc *sch,
							struct tcmsg *tcm)
{
	struct netdev_queue *qdev;

	qdev = mhi_netdev_queue_get(sch, TC_H_MIN(tcm->tcm_parent));

	if (qdev)
		return qdev;

	return netdev_get_tx_queue(qdisc_dev(sch), 0);
}

static int mhi_graft(struct Qdisc *sch, unsigned long cl,
		     struct Qdisc *new, struct Qdisc **old)
{
	struct net_device *dev = qdisc_dev(sch);
	struct netdev_queue *qdev;

	DPRINTK("mhi_graft");

	qdev = mhi_netdev_queue_get(sch, cl);
	BUGGER(qdev == NULL);

	if (dev->flags & IFF_UP)
		dev_deactivate(dev);

	*old = dev_graft_qdisc(qdev, new);

	if (dev->flags & IFF_UP)
		dev_activate(dev);

	return 0;
}

static struct Qdisc *mhi_leaf(struct Qdisc *sch, unsigned long cl)
{
	struct netdev_queue *qdev;

	qdev = mhi_netdev_queue_get(sch, cl);
	BUGGER(qdev == NULL);

	return qdev->qdisc_sleeping;
}

static unsigned long mhi_get(struct Qdisc *sch, u32 classid)
{
	unsigned int i = TC_H_MIN(classid);

	if (!mhi_netdev_queue_get(sch, i))
		return 0;

	return i;
}

static void mhi_put(struct Qdisc *sch, unsigned long cl)
{
	return;
}

static int mhi_dump_class(struct Qdisc *sch, unsigned long cl,
			  struct sk_buff *skb, struct tcmsg *tcm)
{
	struct netdev_queue *qdev;

	qdev = mhi_netdev_queue_get(sch, cl);
	BUGGER(qdev == NULL);

	tcm->tcm_parent  = TC_H_ROOT;
	tcm->tcm_handle |= TC_H_MIN(cl);
	tcm->tcm_info    = qdev->qdisc_sleeping->handle;

	return 0;
}

static int mhi_dump_class_stats(struct Qdisc *sch, unsigned long cl,
				struct gnet_dump *d)
{
	struct netdev_queue *qdev;

	qdev = mhi_netdev_queue_get(sch, cl);
	BUGGER(qdev == NULL);

	sch = qdev->qdisc_sleeping;

	sch->qstats.qlen = sch->q.qlen;

	if (gnet_stats_copy_basic(d, &sch->bstats) < 0 ||
	    gnet_stats_copy_queue(d, &sch->qstats) < 0)
		return -1;

	return 0;
}

static void mhi_walk(struct Qdisc *sch, struct qdisc_walker *arg)
{
	struct mhi_qdisc *priv = qdisc_priv(sch);
	unsigned int i;

	BUGGER(priv == NULL);

	if (arg->stop)
		return;

	arg->count = arg->skip;
	for (i = arg->skip; i < priv->nq; i++) {
		if (arg->fn(sch, i + 1, arg) < 0) {
			arg->stop = 1;
			break;
		}
		arg->count++;
	}
}


/*** Notifier functions ***/

int
mhi_register_queue_notifier(struct Qdisc *sch,
					struct notifier_block *nb,
					unsigned long cl)
{
	struct netdev_queue *qdev;
	struct mhdp_qdisc *leaf;
	struct Qdisc *qdisc;
	int err;

	DPRINTK("mhi_register_queue_notifier for queue:%lu", cl);

	if (sch->ops != &mhi_qdisc_ops) {
		DPRINTK("mhi_register_queue_notifier: FAILED: not an MHI root Qdisc");
		return -1;
	}

	qdev = mhi_netdev_queue_get(sch, cl);
	if (!qdev) {
		DPRINTK("mhi_register_queue_notifier: FAILED: no such queue");
		return -1;
	}

	qdisc = qdev->qdisc_sleeping;

	if (qdisc->ops != &mhi_mhdp_ops) {
		DPRINTK("mhi_register_queue_notifier: FAILED: queue not an MHDP Qdisc");
		return -1;
	}

	leaf = qdisc_priv(qdisc);

	err = srcu_notifier_chain_register(&leaf->qnot, nb);

	if (err)
		DPRINTK("mhi_qdisc_register_notifier: failed: %d", err);

	return err;
}
EXPORT_SYMBOL(mhi_register_queue_notifier);

int
mhi_unregister_queue_notifier(struct Qdisc *sch,
				struct notifier_block *nb,
				unsigned long cl)
{
	struct netdev_queue *qdev;
	struct mhdp_qdisc *leaf;
	struct Qdisc *qdisc;
	int err;

	DPRINTK("mhi_unregister_queue_notifier for queue:%lu", cl);

	if (sch->ops != &mhi_qdisc_ops) {
		DPRINTK("mhi_unregister_queue_notifier: FAILED: not an MHI root Qdisc");
		return -1;
	}

	qdev = mhi_netdev_queue_get(sch, cl);
	if (!qdev) {
		DPRINTK("mhi_register_queue_notifier: FAILED: no such queue");
		return -1;
	}

	qdisc = qdev->qdisc_sleeping;

	if (qdisc->ops != &mhi_mhdp_ops) {
		DPRINTK("mhi_unregister_queue_notifier: FAILED: queue not an MHDP Qdisc");
		return -1;
	}

	leaf = qdisc_priv(qdisc);

	err = srcu_notifier_chain_unregister(&leaf->qnot, nb);

	if (err)
		DPRINTK("mhi_unregister_queue_notifier: failed: %d", err);

	return err;
}
EXPORT_SYMBOL(mhi_unregister_queue_notifier);


/*** Qdisc structures ***/

static struct Qdisc_ops mhi_noq_ops __read_mostly = {
	.id		= "mhi-noq",
	.priv_size	= 0,
	.enqueue	= NULL,
	.dequeue	= null_dequeue,
	.peek		= null_dequeue,
	.owner		= THIS_MODULE,
};

static struct Qdisc_ops mhi_fifo_ops __read_mostly = {
	.id		= "mhi-fifo",
	.priv_size	= 0,
	.enqueue	= qdisc_enqueue_tail,
	.dequeue	= qdisc_dequeue_head,
	.peek		= qdisc_peek_head,
	.drop		= qdisc_queue_drop,
	.reset		= qdisc_reset_queue,
	.owner		= THIS_MODULE,
};

static struct Qdisc_ops mhi_mhdp_ops __read_mostly = {
	.id		= "mhi-mhdp",
	.priv_size	= sizeof(struct mhdp_qdisc),
	.enqueue	= mhdp_enqueue,
	.dequeue	= mhdp_dequeue,
	.peek		= qdisc_peek_head,
	.drop		= mhdp_drop,
	.init		= mhdp_init,
	.reset		= qdisc_reset_queue,
	.destroy	= mhdp_destroy,
	.change		= mhdp_tune,
	.owner		= THIS_MODULE,
};

static const struct Qdisc_class_ops mhi_class_ops = {
	.select_queue	= mhi_select_queue,
	.graft		= mhi_graft,
	.leaf		= mhi_leaf,
	.get		= mhi_get,
	.put		= mhi_put,
	.walk		= mhi_walk,
	.dump		= mhi_dump_class,
	.dump_stats	= mhi_dump_class_stats,
};

static struct Qdisc_ops mhi_qdisc_ops __read_mostly = {
	.cl_ops		= &mhi_class_ops,
	.id		= "mhi",
	.priv_size	= sizeof(struct mhi_qdisc),
	.init		= mhi_init,
	.destroy	= mhi_destroy,
	.attach		= mhi_attach,
	.dump		= mhi_dump,
	.owner		= THIS_MODULE,
};


static struct Qdisc_ops *mhi_queue_ops[] = {
	&mhi_noq_ops,
	&mhi_fifo_ops,
	&mhi_mhdp_ops,
};

#define MHI_NOQ_OPS    0
#define MHI_FIFO_OPS   1
#define MHI_MHDP_OPS   2

static int mhi_qdisc_tab[] = {
	MHI_MHDP_OPS,	/* Priority 0 = User plane */
	MHI_FIFO_OPS,	/* Priority 1 = Control plane */
	MHI_FIFO_OPS,	/* Priority 2 = Audio */
	MHI_NOQ_OPS,	/* Priority 3 = <UNALLOC> */
	MHI_NOQ_OPS,	/* Priority 4 = <UNALLOC> */
	MHI_NOQ_OPS,	/* Priority 5 = <UNALLOC> */
	MHI_NOQ_OPS,	/* Priority 6 = <UNALLOC> */
	MHI_NOQ_OPS,	/* Priority 7 = <UNALLOC> */
};

static int mhi_param_count = 0;


/*** Module functions ***/

static int __init sch_mhi_init(void)
{
	int err;

	if (mhi_param_count > 8)
		return -1;

	err = register_qdisc(&mhi_qdisc_ops);
	if (err)
		goto rollback0;

	err = register_qdisc(&mhi_noq_ops);
	if (err)
		goto rollback1;

	err = register_qdisc(&mhi_fifo_ops);
	if (err)
		goto rollback2;

	err = register_qdisc(&mhi_mhdp_ops);
	if (err)
		goto rollback3;

	return 0;

rollback3:
	unregister_qdisc(&mhi_fifo_ops);
rollback2:
	unregister_qdisc(&mhi_noq_ops);
rollback1:
	unregister_qdisc(&mhi_qdisc_ops);
rollback0:
	return -1;
}

static void __exit sch_mhi_exit(void)
{
	unregister_qdisc(&mhi_mhdp_ops);
	unregister_qdisc(&mhi_fifo_ops);
	unregister_qdisc(&mhi_noq_ops);
	unregister_qdisc(&mhi_qdisc_ops);
}

module_init(sch_mhi_init)
module_exit(sch_mhi_exit)

module_param_array_named(default_qdisc,
						mhi_qdisc_tab,
						int,
						&mhi_param_count,
						0444);

MODULE_AUTHOR("Renesas Mobile Corporation");
MODULE_DESCRIPTION("MHI Queue Discipline");
MODULE_LICENSE("GPL");

