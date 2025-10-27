/*
 * Resource table for TI AM67A Cortex-R5F
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RSC_TABLE_H_
#define RSC_TABLE_H_

#include <stddef.h>
#include <openamp/open_amp.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NO_RESOURCE_ENTRIES         8

struct remote_resource_table {
        uint32_t version;
        uint32_t num;
        uint32_t reserved[2];
        uint32_t offset[NO_RESOURCE_ENTRIES];
        struct fw_rsc_vdev rpmsg_vdev;
        struct fw_rsc_vdev_vring rpmsg_vring0;
        struct fw_rsc_vdev_vring rpmsg_vring1;
        struct fw_rsc_trace rsc_trace;
} __attribute__((packed, aligned(0x100)));

void *get_resource_table(uint32_t rsc_id, uint32_t *len);
char *get_rsc_trace_info(uint32_t *len);

#ifdef __cplusplus
}
#endif

#endif /* RSC_TABLE_H_ */

