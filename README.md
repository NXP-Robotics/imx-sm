The System Manager (SM) is an application that runs on a Cortex-M processor on many NXP i.MX processors.
The Cortex-M is the boot core, runs the boot ROM which loads the SM (and other boot code), and then
branches to the SM. The SM then configures some aspects of the hardware such as isolation mechanisms
and then starts other cores in the system. After starting these cores, it enters a service mode where
it provides access to clocking, power, sensor, and pin control via a client RPC API based on ARM's
[System Control and Management Interface (SCMI)](https://developer.arm.com/documentation/den0056/latest/).
To facilitate isolation between cores, the SM partitions the SoC into logical machines (LM) which have
statically configurable access rights to both hardware and RPC API calls.

Supported development environments include Ubuntu 2020.04, 2022.04, and 2024.04.

**If upgrading, please follow the instructions below to build and review the release notes to understand
required board port and configuration file changes.**

Download from GitHub
====================

To download the repo from [GitHub](https://github.com/nxp-imx/imx-sm), first install git:

    sudo apt update
    sudo apt -y install git

Then clone the repo:

git clone https://github.com/nxp-imx/imx-sm

Building the Documentation
==========================

A comprehensive *SM Reference Manual* can be built from the source. This manual contains an architecture
description, developer's guide, porting guide, configuration guide, protocol description, debugging info,
and implementation details. The preferred output is HTML.

To build the full documentation, install the required tools:

    sudo apt -y install make doxygen texlive texlive-latex-extra

Then change to the documentation directory (sm/doc) and execute the following:

    make clean
    make html

The output is written to the build directory (sm/doc/build/html). The top level for the html output
is the *index.html* file.

To build the release notes:

    make clean
    make rn

The output is sm/doc/sm-rn.pdf (also an html version in sm/doc/build/html).

Supported versions of doxygen include 1.8.17, 1.9.1, and 1.9.8.

Installing the Toolchain
========================

Compiling requires an ARM cross-compiler. Download and install the required arm-none-eabi toolchain from
the [Arm GNU Toolchain Downloads](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
website. For example, from the x86_64 Linux hosted cross toolchains AArch32 bare-metal target
(arm-none-eabi) section, download the *.xz file to a directory for the tool chain (e.g. tools) and then:

    tar xvf *.xz

Set the TOOLS shell variable to the directory the toolchain is installed in. For example:

    export TOOLS=~/tools

When the toolchain is in a directory like arm-gnu-toolchain-\*-x86_64-arm-none-eabi in this
*tools* directory.

Also ensure the Linux installation is up-to-date and then install:

    sudo apt -y install make gcc g++-multilib srecord

See the *Release Notes* for the exact toolchain version the SM was tested with.

Compiling an SM Image
=====================

To get started with the SM on an NXP board, download the [GitHub](https://github.com/nxp-imx/imx-sm)
repository and then compile the SM for the target configuration.

For example, in the top directory of the code base (e.g. imx-sm):

    make config=mx95evk cfg  (required only if the associated mx95evk.cfg file has been changed)
    make config=mx95evk all

Then copy the resulting binary image file, *build/mx95evk/m33_image.bin*, to the iMX95 directory of the
[mkimage tool](https://github.com/nxp-imx/imx-mkimage). Build a boot container using mkimage, program
it to the boot device, and boot the board. The SM will display a debug monitor prompt on the M33 debug
UART.

## Debug console over a SWD debugger (no UART)

On boards where no UART is available for the SM, the debug console (banner, `printf`, and the
interactive debug monitor) can be carried over an attached SWD debug probe instead. Append
`c=rtt` to the build:

    make config=mx95evk c=rtt m=1 all

This uses a pair of RAM ring buffers laid out to match the SEGGER RTT control block, which the
probe reads and writes using background memory access. No pins, no SWO, and no trace clock are
required, and the debug LPUART is left entirely unconfigured (neither the pads nor the peripheral
are touched) so it stays free for another owner. The default remains `c=uart`, so existing builds
are unaffected.

Attach from the host with either:

    JLinkRTTClient
    pyocd rtt -t mimx95_cm33_mx25um -a 0x20020000 -s 0x20000

pyOCD must be given a search range covering the SM data region, as the region it uses by default
(`codetcm` at `0x1ffc0000`) does not contain the control block and it reports
`Control block not found`. The range above is deliberately not the whole TCM: the address space
below the data region is not all accessible and reading it aborts with `Memory transfer fault`.

Notes:

- Output is best effort. Nothing drains the buffer unless a host is reading it, so a full buffer
  is waited on for at most `SM_RTT_DRAIN_USEC` (default 10 ms per write) and only once a host has
  been seen consuming data. Anything that still does not fit is dropped, so with no probe attached
  output is discarded and the SM never stalls.
- `SM_RTT_UP_SIZE` (default 4KB) gives the host room to keep up, so the buffer does not have to
  hold a whole command's output. Verbose commands emit more than this in one burst (`clock.r` is
  around 9KB).
- Attaching or detaching a probe at any time is safe.
- Echo is done by the host, not the SM, which is the RTT convention. `JLinkRTTClient` has
  `-LocalEcho On` by default; pass `-LocalEcho Off` if characters appear twice.
- Waiting for input polls rather than sleeping, as the UART console does. A `WFI` would commit the
  CPU to the sleep mode armed in the GPC, stopping the SysTick that refreshes the watchdog.
- Use `m=1` for interactive use. `m=2` ("press key to enter monitor") works in RUN mode, but the
  keypress is missed while the system is in a deeper sleep state, where the tick is stopped.

The above config is for Linux+M7 system testing. Another config is delivered for MCUXpresso testing:

    make config=mx95alt all

Customers will usually create their own config that partitions resources between the AP and M7 for
their use-case.

Complete documentation on compiling the SM, building a boot image, and programming the boot image is
contained in the *Developer's Guide* section of the full doxygen-based RM described above.

The SM comes with a basic configuration tool. A more comprehensive alternative SM config tool (SMCT)
is available here: [imx-smct](https://github.com/nxp-imx/imx-smct).

