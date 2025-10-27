/*
 * Texas Instruments AM67A Cortex-R5F platform information
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLATFORM_INFO_H_
#define PLATFORM_INFO_H_

#include <stdbool.h>
#include <openamp/remoteproc.h>
#include <openamp/virtio.h>
#include <openamp/rpmsg.h>
#include <metal/log.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BIT
#define BIT(nr)                        (1UL << (nr))
#endif

#ifndef TI_R5F_CORE_INDEX
#define TI_R5F_CORE_INDEX        0U
#endif

#define TI_AM67A_SHARED_MEM_CORE0    0x9C800000UL
#define TI_AM67A_SHARED_MEM_CORE1    0x9E000000UL
#define TI_AM67A_SHARED_MEM_SIZE     (24UL * 1024UL * 1024UL)

#if TI_R5F_CORE_INDEX == 0U
#define SHARED_MEM_PA               TI_AM67A_SHARED_MEM_CORE0
#elif TI_R5F_CORE_INDEX == 1U
#define SHARED_MEM_PA               TI_AM67A_SHARED_MEM_CORE1
#else
#error "Unsupported TI_R5F_CORE_INDEX value"
#endif

#ifndef SHARED_MEM_SIZE
#define SHARED_MEM_SIZE             TI_AM67A_SHARED_MEM_SIZE
#endif

#ifndef SHARED_BUF_OFFSET
#define SHARED_BUF_OFFSET           0x00100000UL
#endif

#ifndef RSC_TRACE_SZ
#define RSC_TRACE_SZ                (4UL * 1024UL)
#endif

#ifndef TI_MAILBOX_BASE
#define TI_MAILBOX_BASE             0x4D410000UL
#endif

#ifndef TI_MAILBOX_REGION_SIZE
#define TI_MAILBOX_REGION_SIZE      0x1000UL
#endif

#ifndef TI_MAILBOX_IRQ
#define TI_MAILBOX_IRQ              36U
#endif

#ifndef TI_MAILBOX_TX_QUEUE
#define TI_MAILBOX_TX_QUEUE         0U
#endif

#ifndef TI_MAILBOX_RX_QUEUE
#define TI_MAILBOX_RX_QUEUE         1U
#endif

#ifndef TI_MAILBOX_IRQ_BIT
#define TI_MAILBOX_IRQ_BIT          (1UL << TI_MAILBOX_RX_QUEUE)
#endif

#define KICK_DEV_NAME               "ti_mailbox"
#define KICK_BUS_NAME               "generic"

/* TI mailbox register offsets */
#define TI_MAILBOX_MESSAGE_OFFSET(queue)        (0x040U + ((queue) * 0x4U))
#define TI_MAILBOX_FIFOSTATUS_OFFSET(queue)     (0x080U + ((queue) * 0x4U))
#define TI_MAILBOX_IRQSTATUS_OFFSET(queue)      (0x0C0U + ((queue) * 0x4U))
#define TI_MAILBOX_IRQENABLE_SET_OFFSET(queue)  (0x0C8U + ((queue) * 0x4U))
#define TI_MAILBOX_IRQENABLE_CLR_OFFSET(queue)  (0x0CCU + ((queue) * 0x4U))

#define TI_MAILBOX_FIFOSTATUS_FULL_MASK         BIT(0)
#define TI_MAILBOX_FIFOSTATUS_EMPTY_MASK        BIT(1)

struct remoteproc_priv {
        const char *mailbox_dev_name;
        const char *mailbox_dev_bus_name;
        struct metal_device *mailbox_dev;
        struct metal_io_region *mailbox_io;
        uint32_t mailbox_irq;
        uint32_t mailbox_tx_queue;
        uint32_t mailbox_rx_queue;
        uint32_t mailbox_irq_mask;
        volatile bool mailbox_pending;
};

struct rproc_plat_info {
        struct rpmsg_device *rpdev;
        struct remoteproc *rproc;
};

int32_t platform_init(int32_t argc, char *argv[], void **platform);
struct rpmsg_device *platform_create_rpmsg_vdev(void *platform,
                uint32_t vdev_index, uint32_t role,
                void (*rst_cb)(struct virtio_device *vdev),
                rpmsg_ns_bind_cb ns_bind_cb);
int32_t platform_poll(void *platform);
int32_t platform_poll_on_vdev_reset(void *arg);
void platform_release_rpmsg_vdev(struct rpmsg_device *rpdev, void *platform);
void platform_cleanup(void *platform);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_INFO_H_ */
