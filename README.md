Driver for the General Standards 16-bit Analog I/O card.

Author: John Quach
This is a proof of concept Linux driver for the General Standards PCI 16AIO board that demonstrates
the creation of a driver that can be registered with the Linux kernel.  

The driver does the following:
Detect the board by reading the PCI configuration space, remap the PCI BAR addresses to access the 
PLX 9080 PCI interface chip and the local registers on the 16AIO, initialize the registers and finally
perform a simple DMA transfer from system memory to PCI memory.

This kernel driver does not implement calls needed for a user mode driver, it is not the intended scope 
for this project.  Also, while interrupts shouldn't be hard to add, it was not added due to time 
constraints.  A complete DMA solution would have interrupts, so that the CPU knows when DMA transfers 
are completed.  Otherwise without it, you'd have to poll the device for completion status, making the
advantages of DMA moot.

The DMA transfer demonstrated here is a 32-bit one-shot transfer initiated by the CPU by setting a CMD 
DMA register in the 16AIO's PLX PCI chip.  The DMA is a transfer from system memory to PCI memory.  
First, a DMA buffer is allocated using "kmalloc" with the GFP_DMA flag, so that memory is in the DMA 
zone.  Then, using pci_map_single(), we get the physical memory pointer to the DMA buffer we allocated
in virtual memory.  With the pointer to the DMA's physical location, the PLX's DMA engine's registers 
are configured to copy the contents of the system memory to an arbitrary register in the PCI card.  I 
chose the card's Output Data Buffer Control Register (0x1C) since it's a 32-bit read/write register.  

With the system configured for DMA, I set the DMA buffer to 0x2222 and send the command to start the 
DMA transfer. The 16AIO PCI card becomes Bus Master and copies the system memory. I then read out the 
0x1C register to verify that the 0x2222 value was written to the card's memory.  

Driver is written based on:
1) pmc16aio_man_063010.pdf User's Manual for the 16AIO Card
2) Broadcom PLX 9080 Data Book
3) Linux Device Drivers, Third Edition, by Jonathan Corbert, Alesssandro Rubini,
		Greg Kroah-Hartman.  lwn.net/Kernel/LDD3
4) Dynamic DMA mapping Guide - https://www.kernel.org/doc/Documentation/DMA-API-HOWTO.txt