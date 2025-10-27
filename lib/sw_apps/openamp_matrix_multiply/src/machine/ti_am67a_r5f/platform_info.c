/*
 * Texas Instruments AM67A Cortex-R5F platform integration for OpenAMP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <metal/device.h>
#include <metal/irq.h>
#include <metal/io.h>
#include <metal/utilities.h>
#include <openamp/rpmsg_virtio.h>
#include <stdlib.h>
#include <string.h>
#include "platform_info.h"
#include "rsc_table.h"

#ifndef RPMSG_NO_IPI
#define _rproc_wait()   __asm volatile("wfi")
#endif

static metal_phys_addr_t mailbox_phys_addr = TI_MAILBOX_BASE;
static struct metal_device mailbox_device = {
        .name = KICK_DEV_NAME,
        .bus = NULL,
        .num_regions = 1,
        .regions = {
                {
                        .virt = (void *)TI_MAILBOX_BASE,
                        .physmap = &mailbox_phys_addr,
                        .size = TI_MAILBOX_REGION_SIZE,
                        .page_shift = -1UL,
                        .page_mask = -1UL,
                        .mem_flags = DEVICE_NONSHARED | PRIV_RW_USER_RW,
                        .ops = {NULL},
                },
        },
        .node = {NULL},
#ifndef RPMSG_NO_IPI
        .irq_num = 1,
        .irq_info = (void *)(uintptr_t)TI_MAILBOX_IRQ,
#endif
};

static struct remoteproc_priv rproc_priv = {
        .mailbox_dev_name = KICK_DEV_NAME,
        .mailbox_dev_bus_name = KICK_BUS_NAME,
        .mailbox_dev = NULL,
        .mailbox_io = NULL,
        .mailbox_irq = TI_MAILBOX_IRQ,
        .mailbox_tx_queue = TI_MAILBOX_TX_QUEUE,
        .mailbox_rx_queue = TI_MAILBOX_RX_QUEUE,
        .mailbox_irq_mask = TI_MAILBOX_IRQ_BIT,
        .mailbox_pending = false,
};

static struct remoteproc rproc_inst;

extern int32_t init_system(void);
extern void cleanup_system(void);

extern const struct remoteproc_ops ti_am67a_r5f_a53_proc_ops;

static struct rpmsg_virtio_shm_pool shpool;

static void ti_mailbox_ack_irq(struct remoteproc_priv *priv)
{
        metal_io_write32(priv->mailbox_io,
                         TI_MAILBOX_IRQSTATUS_OFFSET(priv->mailbox_rx_queue),
                         priv->mailbox_irq_mask);
}

static int ti_mailbox_get_status(struct remoteproc_priv *priv)
{
        return metal_io_read32(priv->mailbox_io,
                               TI_MAILBOX_IRQSTATUS_OFFSET(priv->mailbox_rx_queue)) &
                priv->mailbox_irq_mask;
}

static struct remoteproc *platform_create_proc(uint32_t proc_index,
                                              uint32_t rsc_index)
{
        void *rsc_table;
        uint32_t rsc_size;
        int32_t ret;
        metal_phys_addr_t pa;

        (void)proc_index;

        rsc_table = get_resource_table(rsc_index, &rsc_size);
        if (!rsc_table)
                return NULL;

        ret = metal_register_generic_device(&mailbox_device);
        if (ret && ret != -EEXIST)
                return NULL;

        if (!remoteproc_init(&rproc_inst, &ti_am67a_r5f_a53_proc_ops, &rproc_priv))
                return NULL;

        pa = (metal_phys_addr_t)rsc_table;
        (void)remoteproc_mmap(&rproc_inst, &pa, NULL, rsc_size,
                              NORM_NSHARED_NCACHE | PRIV_RW_USER_RW,
                              &rproc_inst.rsc_io);

        pa = SHARED_MEM_PA;
        (void)remoteproc_mmap(&rproc_inst, &pa, NULL, SHARED_MEM_SIZE,
                              NORM_NSHARED_NCACHE | PRIV_RW_USER_RW, NULL);

        ret = remoteproc_set_rsc_table(&rproc_inst, rsc_table, rsc_size);
        if (ret != 0) {
                remoteproc_remove(&rproc_inst);
                return NULL;
        }

        return &rproc_inst;
}

int32_t platform_init(int32_t argc, char *argv[], void **platform)
{
        unsigned long proc_id = 0;
        unsigned long rsc_id = 0;
        struct remoteproc *rproc;

        if (!platform)
                return -EINVAL;

        init_system();

        if (argc >= 2)
                proc_id = strtoul(argv[1], NULL, 0);

        if (argc >= 3)
                rsc_id = strtoul(argv[2], NULL, 0);

        rproc = platform_create_proc(proc_id, rsc_id);
        if (!rproc)
                return -EINVAL;

        *platform = rproc;
        return 0;
}

struct rpmsg_device *platform_create_rpmsg_vdev(void *platform,
                uint32_t vdev_index, uint32_t role,
                void (*rst_cb)(struct virtio_device *vdev),
                rpmsg_ns_bind_cb ns_bind_cb)
{
        struct remoteproc *rproc = platform;
        struct rpmsg_virtio_device *rpmsg_vdev;
        struct virtio_device *vdev;
        void *shbuf;
        struct metal_io_region *shbuf_io;
        int ret;

        rpmsg_vdev = metal_allocate_memory(sizeof(*rpmsg_vdev));
        if (!rpmsg_vdev)
                return NULL;

        shbuf_io = remoteproc_get_io_with_pa(rproc, SHARED_MEM_PA);
        if (!shbuf_io)
                goto err1;

        shbuf = metal_io_phys_to_virt(shbuf_io,
                                      SHARED_MEM_PA + SHARED_BUF_OFFSET);

        vdev = remoteproc_create_virtio(rproc, vdev_index, role, rst_cb);
        if (!vdev)
                goto err1;

        rpmsg_virtio_init_shm_pool(&shpool, shbuf,
                                   (SHARED_MEM_SIZE - SHARED_BUF_OFFSET));

        ret = rpmsg_init_vdev(rpmsg_vdev, vdev, ns_bind_cb, shbuf_io, &shpool);
        if (ret != 0)
                goto err2;

        return rpmsg_virtio_get_rpmsg_device(rpmsg_vdev);
err2:
        remoteproc_remove_virtio(rproc, vdev);
err1:
        metal_free_memory(rpmsg_vdev);
        return NULL;
}

int32_t platform_poll_on_vdev_reset(void *arg)
{
        struct rproc_plat_info *data = arg;
        struct rpmsg_device *rpdev = data->rpdev;
        struct rpmsg_virtio_device *rvdev;
        struct remoteproc *rproc = data->rproc;
        struct remoteproc_priv *priv;

        if (!rproc || !rpdev)
                return -EINVAL;

        priv = rproc->priv;
        if (!priv)
                return -EINVAL;

        rvdev = metal_container_of(rpdev, struct rpmsg_virtio_device, rdev);

        while (rpmsg_virtio_get_status(rvdev) & VIRTIO_CONFIG_STATUS_DRIVER_OK) {
#ifndef RPMSG_NO_IPI
                if (priv->mailbox_pending || ti_mailbox_get_status(priv)) {
                        ti_mailbox_ack_irq(priv);
                        priv->mailbox_pending = false;
                        remoteproc_get_notification(rproc, RSC_NOTIFY_ID_ANY);
                }
                _rproc_wait();
#else
                (void)priv;
#endif
        }

        return 0;
}

int32_t platform_poll(void *priv_data)
{
        struct remoteproc *rproc = priv_data;
        struct remoteproc_priv *priv;
        int32_t ret;

        if (!rproc)
                return -EINVAL;

        priv = rproc->priv;
        if (!priv)
                return -EINVAL;

        while (1) {
#ifndef RPMSG_NO_IPI
                if (priv->mailbox_pending || ti_mailbox_get_status(priv)) {
                        ti_mailbox_ack_irq(priv);
                        priv->mailbox_pending = false;
                        ret = remoteproc_get_notification(rproc,
                                                          RSC_NOTIFY_ID_ANY);
                        if (ret != 0)
                                return ret;
                        break;
                }
                _rproc_wait();
#else
                (void)priv;
                break;
#endif
        }

        return 0;
}

void platform_release_rpmsg_vdev(struct rpmsg_device *rpdev, void *platform)
{
        struct rpmsg_virtio_device *rpvdev;
        struct remoteproc *rproc;

        rpvdev = metal_container_of(rpdev, struct rpmsg_virtio_device, rdev);
        rproc = platform;

        rpmsg_deinit_vdev(rpvdev);
        remoteproc_remove_virtio(rproc, rpvdev->vdev);
}

void platform_cleanup(void *platform)
{
        struct remoteproc *rproc = platform;

        if (rproc)
                remoteproc_remove(rproc);

        cleanup_system();
}

