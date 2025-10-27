# TI AM67A Dual R5F OpenAMP Integration Summary

## Goals

* Provide first-class OpenAMP support for both Cortex-R5F cores on the AM67A
  (BeagleY-AI) platform.
* Replace Xilinx-specific IPI dependencies with the TI K3 mailbox subsystem.
* Support both standalone and FreeRTOS firmware builds from the existing
  OpenAMP matrix multiplication template.

## Key Changes

1. **Machine layer** – Added a new `ti_am67a_r5f` machine with platform, resource
   table, and remoteproc glue sources. The mailbox driver maps queue pairs to
   VirtIO notifications and exposes a configurable interrupt vector.
2. **System helpers** – Implemented generic and FreeRTOS helpers that initialise
   the GIC-500, install libmetal logging backed by the 4&nbsp;KiB trace buffer, and
   wake the R5F redistributor before enabling interrupts.
3. **Configuration** – Updated the application YAML so XSCT and the cmake build
   system list `ti_am67a_r5f` and `ti_j722s_r5f` as supported processors.
4. **Documentation** – Added a detailed platform guide, quickstart, and internal
   summary covering memory maps, lockstep considerations, and testing results.

## Notable Implementation Details

* **Mailbox abstraction** – The mailbox interrupt handler acknowledges the
  receive queue, toggles a pending flag, and leaves vring processing to the
  platform polling loop. This keeps interrupt latency low and mirrors the
  behaviour on Zynq platforms while using TI registers (`MESSAGE`, `FIFOSTATUS`,
  `IRQSTATUS`, `IRQENABLE`).
* **Shared memory offsets** – Each core reserves the top 1&nbsp;MiB of its 24&nbsp;MiB
  window for rpmsg buffers (`SHARED_BUF_OFFSET`). The remainder is available to
  the resource table and application payloads.
* **Trace logging** – Both helpers install a libmetal log handler that writes to
  the resource table trace buffer and the default log backend (UART or console),
  enabling capture through Linux `debugfs` without additional instrumentation.
* **Interrupt routing** – Minimal GIC configuration ensures the mailbox IRQ is
  enabled regardless of whether it lands in the redistributor (IDs < 32) or in
  the main distributor (IDs ≥ 32). The router register is programmed with zero
  affinity, allowing device tree affinity overrides.

## Compatibility Notes

* The new machine reuses existing OpenAMP abstractions; no changes were required
  in the core library.
* Mailbox, shared memory, and interrupt parameters can be overridden at compile
  time to match board-specific device tree values without editing source files.
* The feature set remains backwards compatible with existing ZynqMP, Versal, and
  Versal NET platforms.

