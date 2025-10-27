/*
 * Generic system helper for TI AM67A Cortex-R5F OpenAMP demo
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <metal/io.h>
#include <metal/irq.h>
#include <metal/log.h>
#include <metal/sys.h>
#include "platform_info.h"

#define TI_GIC_DIST_BASE          0x01800000UL
#define TI_GIC_REDIST_BASE        0x01880000UL

#define GICD_CTLR                 0x000
#define GICD_ISENABLER(n)         (0x100 + ((n) * 4U))
#define GICD_IPRIORITYR(n)        (0x400 + (n))
#define GICD_IROUTER(n)           (0x6000 + ((n) * 8U))

#define GICR_WAKER                0x0014
#define GICR_IGROUPR0             0x0080
#define GICR_ISENABLER0           0x0100
#define GICR_IPRIORITYR(n)        (0x0400 + (n))

#define DEFAULT_IRQ_PRIORITY      0xA0U
#define GICR_WAKER_PROCESSOR_ASLEEP_MASK    BIT(1)
#define GICR_WAKER_CHILDREN_ASLEEP_MASK     BIT(2)

static inline void ti_gic_write32(uintptr_t addr, uint32_t value)
{
        *(volatile uint32_t *)addr = value;
}

static inline uint32_t ti_gic_read32(uintptr_t addr)
{
        return *(volatile uint32_t *)addr;
}

static inline void ti_gic_write64(uintptr_t addr, uint64_t value)
{
        *(volatile uint64_t *)addr = value;
}

static void ti_gic_wake_redist(void)
{
        uintptr_t base = TI_GIC_REDIST_BASE;
        uint32_t waker;

        waker = ti_gic_read32(base + GICR_WAKER);
        waker &= ~GICR_WAKER_PROCESSOR_ASLEEP_MASK;
        ti_gic_write32(base + GICR_WAKER, waker);

        while (ti_gic_read32(base + GICR_WAKER) & GICR_WAKER_CHILDREN_ASLEEP_MASK)
                ;
}

static void ti_gic_configure_irq(uint32_t irq)
{
        uint32_t group_idx;
        uint32_t enable_idx;
        uint32_t priority_idx;

        if (irq < 32U) {
                ti_gic_write32(TI_GIC_REDIST_BASE + GICR_IGROUPR0, 0U);
                ti_gic_write32(TI_GIC_REDIST_BASE + GICR_ISENABLER0,
                               BIT(irq));
                ti_gic_write32(TI_GIC_REDIST_BASE + GICR_IPRIORITYR(irq),
                               DEFAULT_IRQ_PRIORITY);
                return;
        }

        group_idx = irq / 32U;
        enable_idx = irq / 32U;
        priority_idx = irq;

        ti_gic_write32(TI_GIC_DIST_BASE + GICD_CTLR, 3U);
        ti_gic_write32(TI_GIC_DIST_BASE + GICD_ISENABLER(enable_idx),
                       BIT(irq % 32U));
        ti_gic_write32(TI_GIC_DIST_BASE + GICD_IPRIORITYR(priority_idx),
                       DEFAULT_IRQ_PRIORITY);
        ti_gic_write64(TI_GIC_DIST_BASE + GICD_IROUTER(irq), 0U);
        ti_gic_write32(TI_GIC_DIST_BASE + 0x080 + (group_idx * 4U), 0U);
}

/* Trace buffer support */
extern char *get_rsc_trace_info(uint32_t *len);

static struct {
        char *buffer;
        uint32_t length;
        uint32_t position;
        uint32_t counter;
} trace_ctx;

static void trace_putchar(char c)
{
        if (!trace_ctx.buffer || trace_ctx.length == 0U)
                return;

        if (trace_ctx.position >= trace_ctx.length)
                trace_ctx.position = 0U;

        trace_ctx.buffer[trace_ctx.position++] = c;
}

static void trace_logger(enum metal_log_level level, const char *fmt, ...)
{
        char msg[128];
        va_list args;
        int len;
        char *p;

        len = snprintf(msg, sizeof(msg), "%u L%u ", trace_ctx.counter++, level);
        if (len < 0 || len >= (int)sizeof(msg))
                len = 0;

        va_start(args, fmt);
        vsnprintf(msg + len, sizeof(msg) - len, fmt, args);
        va_end(args);

        for (p = msg; *p && (p - msg) < (int)sizeof(msg); ++p)
                trace_putchar(*p);

        printf("%s", msg);
}

int32_t init_system(void)
{
        struct metal_init_params params = METAL_INIT_DEFAULTS;
        uint32_t len;

        trace_ctx.buffer = get_rsc_trace_info(&len);
        trace_ctx.length = len;
        trace_ctx.position = 0U;
        trace_ctx.counter = 0U;

        if (trace_ctx.buffer && trace_ctx.length) {
                params.log_handler = trace_logger;
                params.log_level = METAL_LOG_DEBUG;
        }

        if (metal_init(&params) != 0)
                return -EINVAL;

        ti_gic_wake_redist();
        ti_gic_configure_irq(TI_MAILBOX_IRQ);

        return 0;
}

void cleanup_system(void)
{
        metal_finish();
}

