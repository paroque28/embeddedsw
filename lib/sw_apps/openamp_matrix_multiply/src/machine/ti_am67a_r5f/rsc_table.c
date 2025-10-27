/*
 * Resource table for TI AM67A Cortex-R5F
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <openamp/open_amp.h>
#include "rsc_table.h"

#define __section_t(S)          __attribute__((__section__(#S)))
#define __resource              __section_t(.resource_table)

#define RPMSG_VDEV_DFEATURES        (1 << VIRTIO_RPMSG_F_NS)
#define VIRTIO_ID_RPMSG_            7
#define NUM_VRINGS                  0x02
#define VRING_ALIGN                 0x1000

#ifndef RING_TX
#define RING_TX                     FW_RSC_U32_ADDR_ANY
#endif

#ifndef RING_RX
#define RING_RX                     FW_RSC_U32_ADDR_ANY
#endif

#define VRING_SIZE                  256
#define NUM_TABLE_ENTRIES           2

static char rsc_trace_buf[RSC_TRACE_SZ];

struct remote_resource_table __resource resources = {
        .version = 1,
        .num = NUM_TABLE_ENTRIES,
        .reserved = {0, 0},
        .offset = {
                [0] = offsetof(struct remote_resource_table, rpmsg_vdev),
                [1] = offsetof(struct remote_resource_table, rsc_trace),
        },
        .rpmsg_vdev = {
                .type = RSC_VDEV,
                .id = VIRTIO_ID_RPMSG_,
                .notifyid = 30,
                .dfeatures = RPMSG_VDEV_DFEATURES,
                .gfeatures = 0,
                .config_len = 0,
                .status = 0,
                .num_of_vrings = NUM_VRINGS,
                .reserved = {0, 0},
        },
        .rpmsg_vring0 = {RING_TX, VRING_ALIGN, VRING_SIZE, 1, 0},
        .rpmsg_vring1 = {RING_RX, VRING_ALIGN, VRING_SIZE, 2, 0},
        .rsc_trace = {
                .type = RSC_TRACE,
                .da = (uint32_t)rsc_trace_buf,
                .len = sizeof(rsc_trace_buf),
                .reserved = 0,
                .name = "ti_r5f_trace",
        },
};

char *get_rsc_trace_info(uint32_t *len)
{
        *len = sizeof(rsc_trace_buf);
        return rsc_trace_buf;
}

void *get_resource_table(uint32_t rsc_id, uint32_t *len)
{
        (void)rsc_id;
        *len = sizeof(resources);
        return &resources;
}

