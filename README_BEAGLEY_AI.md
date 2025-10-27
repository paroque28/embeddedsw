# BeagleY-AI Dual R5F OpenAMP Platform Guide

This document describes how to run the OpenAMP matrix multiplication demo on
the Texas Instruments AM67A (J722S) SoC that powers the BeagleY-AI board. The
platform supports both Cortex-R5F cores in split mode and can be configured for
lockstep execution when required by safety-critical applications.

## 1. Architecture Overview

* **Processing cores** – Dual Cortex-R5F cores clocked at 800&nbsp;MHz. The cores
  can operate independently (split mode) or as a redundant pair (lockstep).
* **Shared memory layout** – Each core receives a 24&nbsp;MiB DDR carve-out defined
  in the Linux device tree. Core 0 uses `0x9C80_0000` and Core 1 uses
  `0x9E00_0000` as the base of its OpenAMP shared memory window.
* **Inter-processor communication** – TI K3 mailbox hardware is used instead of
  the Xilinx IPI controller. The implementation relies on the mailbox message,
  FIFO status, IRQ status, and IRQ enable registers to synchronize with the
  Cortex-A53 application processors.
* **Interrupt routing** – The GIC-500 distributor and redistributor are
  configured so that the mailbox interrupt reaches the targeted R5F core in
  split mode. In lockstep mode the same interrupt can be routed to the lockstep
  pair.
* **VirtIO configuration** – RPMsg uses 256-descriptor vrings backed by the
  24&nbsp;MiB shared memory region. A 4&nbsp;KiB trace buffer allows remote logging via
  Linux `debugfs`.

## 2. Software Overview

* **Operating systems** – Both standalone and FreeRTOS environments are
  supported. Common platform code lives in `lib/sw_apps/openamp_matrix_multiply`
  under the new `ti_am67a_r5f` machine directory.
* **Mailboxes** – The mailbox driver sends a 32-bit message when a vring needs
  service. The receive side acks and clears the interrupt through the mailbox
  IRQ status register and polls the vrings until the VirtIO state machine
  reports that the host detached.
* **Resource table** – The resource table exposes a VirtIO RPMsg device,
  vrings, and the trace buffer. The `notifyid` value defaults to `30` and can be
  overridden in the device tree.

## 3. Build Instructions

1. **Linux host prerequisites** – The AM67A Linux image must expose the R5F
   remote processors via the standard `remoteproc` framework. Ensure the device
   tree contains the per-core memory carve-outs and sets
   `ti,cluster-mode = <0>` for split mode.
2. **Standalone demo** – Build the firmware using XSCT or CMake in the SDK:
   ```bash
   xsct> hsi open_hw design.tcl
   xsct> setws build/beagley_ai
   xsct> app create -name matrix_r5f0 -hw hardware_platform \
        -os standalone -proc ti_am67a_r5f -template {OpenAMP matrix multiplication}
   xsct> app build matrix_r5f0
   ```
   Repeat with `-proc ti_j722s_r5f` to target the second core.
3. **FreeRTOS demo** – Replace `-os standalone` with `-os freertos10_xilinx` and
   rebuild. The helper sources select the correct interrupt controller and
   logging backend automatically.
4. **Deploying firmware** – Copy the resulting ELF files to `/lib/firmware` on
   the Linux filesystem and load them with:
   ```bash
   echo matrix_r5f0.elf > /sys/class/remoteproc/remoteprocX/firmware
   echo start > /sys/class/remoteproc/remoteprocX/state
   ```
5. **Testing** – Use `openamp-rpmsg` userspace utilities or the provided matrix
   multiply host application to exchange data via `/dev/rpmsg0` and
   `/dev/rpmsg1`.

## 4. Device Tree Notes

```dts
r5fss0: r5fss@90d00000 {
        ti,cluster-mode = <0>;              /* split mode */
        memory-region = <&r5f0_core0_dma_pool>;

        r5f0_core0: r5f@0 {
                firmware-name = "matrix_r5f0.elf";
                ti,mbox = <&mailbox0 0 1>;  /* A53 -> R5F */
                memory-region = <&r5f0_core0_dma_pool>;
        };

        r5f0_core1: r5f@1 {
                firmware-name = "matrix_r5f1.elf";
                ti,mbox = <&mailbox0 1 0>;  /* A53 -> R5F */
                memory-region = <&r5f0_core1_dma_pool>;
        };
};
```

Adjust the mailbox indices if your Linux kernel uses different queue pairs.
The shared memory carve-outs must match the `SHARED_MEM_PA` definitions in the
firmware.

## 5. Debugging Tips

* Enable trace logging by reading `/sys/kernel/debug/remoteproc/remoteprocX/trace0`.
* Use `dmesg` to confirm that both remote processors started and that RPMsg
  devices were registered.
* When running in lockstep mode make sure both firmware images are identical.
  The remoteproc framework loads the image into both halves automatically.
* If mailbox interrupts do not fire, verify that the GIC `IROUTER` targets the
  active R5F core and that the Linux side enabled the complementary mailbox
  interrupt.

