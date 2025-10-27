# BeagleY-AI Dual R5F Quickstart

This short guide walks through validating the OpenAMP matrix multiplication
example on both Cortex-R5F cores of the BeagleY-AI board.

## Prerequisites

* BeagleY-AI board running a recent TI Processor SDK Linux release.
* Linux remoteproc support enabled for both R5F instances.
* Pre-built OpenAMP matrix multiplication host application or `rpmsg_char`
  tools on the Cortex-A53 side.

## 1. Prepare shared memory and mailbox configuration

1. Ensure the device tree contains the 24&nbsp;MiB carve-outs for each core:
   * Core 0: `0x9C80_0000` – `0x9DFF_FFFF`
   * Core 1: `0x9E00_0000` – `0x9F7F_FFFF`
2. Set `ti,cluster-mode = <0>` in the `r5fss` node to enable split mode.
3. Bind the R5F mailboxes to the A53 cluster (for example, `<&mailbox0 0 1>` for
   core 0 and `<&mailbox0 1 0>` for core 1).

## 2. Build firmware

1. Launch XSCT or use CMake to create two applications using the
   **OpenAMP matrix multiplication** template.
2. Select processor `ti_am67a_r5f` for core 0 and `ti_j722s_r5f` for core 1.
3. Build the projects (`app build matrix_r5f0`, `app build matrix_r5f1`).

## 3. Deploy firmware

1. Copy `matrix_r5f0.elf` and `matrix_r5f1.elf` into `/lib/firmware`.
2. Start each remote processor:
   ```bash
   echo matrix_r5f0.elf | sudo tee /sys/class/remoteproc/remoteproc0/firmware
   echo start | sudo tee /sys/class/remoteproc/remoteproc0/state

   echo matrix_r5f1.elf | sudo tee /sys/class/remoteproc/remoteproc1/firmware
   echo start | sudo tee /sys/class/remoteproc/remoteproc1/state
   ```

## 4. Run the demo

1. On the host, open `/dev/rpmsg0` and `/dev/rpmsg1` (one device per R5F core).
2. Send two `6x6` matrices to each endpoint. The remote firmware multiplies the
   matrices and returns the result.
3. Repeat the test while monitoring the trace buffer:
   ```bash
   sudo cat /sys/kernel/debug/remoteproc/remoteproc0/trace0
   sudo cat /sys/kernel/debug/remoteproc/remoteproc1/trace0
   ```

## 5. Troubleshooting

* If `/dev/rpmsg*` nodes do not appear, check `dmesg` for remoteproc errors.
* Confirm the mailbox interrupt numbers align with the firmware configuration
  (`TI_MAILBOX_IRQ` in `platform_info.h`).
* For lockstep validation, load the same ELF on both cores and set
  `ti,cluster-mode = <1>` in the device tree.

