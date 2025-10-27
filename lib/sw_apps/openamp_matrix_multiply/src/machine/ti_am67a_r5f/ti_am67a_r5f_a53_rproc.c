/*
 * Texas Instruments AM67A Cortex-R5F remoteproc glue
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <metal/device.h>
#include <metal/irq.h>
#include <metal/io.h>
#include <openamp/remoteproc.h>
#include <string.h>
#include "platform_info.h"

static void ti_mailbox_enable_rx_irq(struct remoteproc_priv *priv)
{
        metal_io_write32(priv->mailbox_io,
                         TI_MAILBOX_IRQENABLE_SET_OFFSET(priv->mailbox_rx_queue),
                         priv->mailbox_irq_mask);
}

static void ti_mailbox_disable_rx_irq(struct remoteproc_priv *priv)
{
        metal_io_write32(priv->mailbox_io,
                         TI_MAILBOX_IRQENABLE_CLR_OFFSET(priv->mailbox_rx_queue),
                         priv->mailbox_irq_mask);
}

static void ti_mailbox_ack_irq(struct remoteproc_priv *priv)
{
        metal_io_write32(priv->mailbox_io,
                         TI_MAILBOX_IRQSTATUS_OFFSET(priv->mailbox_rx_queue),
                         priv->mailbox_irq_mask);
}

static bool ti_mailbox_fifo_full(struct remoteproc_priv *priv)
{
        uint32_t status;

        status = metal_io_read32(priv->mailbox_io,
                                 TI_MAILBOX_FIFOSTATUS_OFFSET(priv->mailbox_tx_queue));
        return (status & TI_MAILBOX_FIFOSTATUS_FULL_MASK) != 0U;
}

static int ti_mailbox_irq_handler(int vect_id, void *data)
{
        struct remoteproc *rproc = data;
        struct remoteproc_priv *priv;

        (void)vect_id;
        if (!rproc)
                return METAL_IRQ_NOT_HANDLED;

        priv = rproc->priv;
        if (!priv)
                return METAL_IRQ_NOT_HANDLED;

        if ((metal_io_read32(priv->mailbox_io,
                              TI_MAILBOX_IRQSTATUS_OFFSET(priv->mailbox_rx_queue)) &
             priv->mailbox_irq_mask) == 0U)
                return METAL_IRQ_NOT_HANDLED;

        ti_mailbox_ack_irq(priv);
        priv->mailbox_pending = true;

        return METAL_IRQ_HANDLED;
}

static struct remoteproc *ti_am67a_r5f_a53_proc_init(struct remoteproc *rproc,
                struct remoteproc_ops *ops, void *arg)
{
        struct remoteproc_priv *priv = arg;
        struct metal_device *mailbox_dev = NULL;
        int ret;

        if (!rproc || !priv || !ops)
                return NULL;

        ret = metal_device_open(priv->mailbox_dev_bus_name,
                                priv->mailbox_dev_name, &mailbox_dev);
        if (ret != 0)
                return NULL;

        priv->mailbox_dev = mailbox_dev;
        priv->mailbox_io = metal_device_io_region(mailbox_dev, 0);
        if (!priv->mailbox_io) {
                metal_device_close(mailbox_dev);
                return NULL;
        }

        priv->mailbox_pending = false;

        metal_irq_register(priv->mailbox_irq, ti_mailbox_irq_handler, rproc);
        metal_irq_enable(priv->mailbox_irq);
        ti_mailbox_enable_rx_irq(priv);

        rproc->ops = ops;
        rproc->priv = priv;

        return rproc;
}

static void ti_am67a_r5f_a53_proc_remove(struct remoteproc *rproc)
{
        struct remoteproc_priv *priv;

        if (!rproc)
                return;

        priv = rproc->priv;
        if (!priv)
                return;

        ti_mailbox_disable_rx_irq(priv);
        metal_irq_disable(priv->mailbox_irq);
        metal_irq_unregister(priv->mailbox_irq);

        if (priv->mailbox_dev)
                metal_device_close(priv->mailbox_dev);
}

static void *ti_am67a_r5f_a53_proc_mmap(struct remoteproc *rproc,
                metal_phys_addr_t *pa, metal_phys_addr_t *da, size_t size,
                unsigned int attribute, struct metal_io_region **io)
{
        struct remoteproc_mem *mem;
        metal_phys_addr_t lpa, lda;
        struct metal_io_region *tmpio;

        lpa = *pa;
        lda = *da;

        if (lpa == METAL_BAD_PHYS && lda == METAL_BAD_PHYS)
                return NULL;

        if (lpa == METAL_BAD_PHYS)
                lpa = lda;
        if (lda == METAL_BAD_PHYS)
                lda = lpa;

        if (!attribute)
                attribute = NORM_SHARED_NCACHE | PRIV_RW_USER_RW;

        mem = metal_allocate_memory(sizeof(*mem));
        if (!mem)
                return NULL;

        tmpio = metal_allocate_memory(sizeof(*tmpio));
        if (!tmpio) {
                metal_free_memory(mem);
                return NULL;
        }

        memset(mem, 0, sizeof(*mem));
        memset(tmpio, 0, sizeof(*tmpio));

        remoteproc_init_mem(mem, NULL, lpa, lda, size, tmpio);
        metal_io_init(tmpio, (void *)lpa, &mem->pa, size,
                      sizeof(metal_phys_addr_t) << 3, attribute, NULL);
        remoteproc_add_mem(rproc, mem);

        *pa = lpa;
        *da = lda;
        if (io)
                *io = tmpio;

        return metal_io_phys_to_virt(tmpio, mem->pa);
}

static int ti_am67a_r5f_a53_proc_notify(struct remoteproc *rproc, uint32_t id)
{
        struct remoteproc_priv *priv;

        if (!rproc)
                return -EINVAL;

        priv = rproc->priv;
        if (!priv)
                return -EINVAL;

        if (ti_mailbox_fifo_full(priv))
                return -EAGAIN;

        metal_io_write32(priv->mailbox_io,
                         TI_MAILBOX_MESSAGE_OFFSET(priv->mailbox_tx_queue), id);

        return 0;
}

const struct remoteproc_ops ti_am67a_r5f_a53_proc_ops = {
        .init = ti_am67a_r5f_a53_proc_init,
        .remove = ti_am67a_r5f_a53_proc_remove,
        .mmap = ti_am67a_r5f_a53_proc_mmap,
        .notify = ti_am67a_r5f_a53_proc_notify,
        .start = NULL,
        .stop = NULL,
        .shutdown = NULL,
};

