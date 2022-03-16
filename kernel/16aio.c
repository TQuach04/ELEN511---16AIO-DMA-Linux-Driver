#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/eventfd.h>
#include <linux/ctype.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/syscalls.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/string.h>
#include <generated/autoconf.h>

// Author: John Quach
// This is a proof of concept Linux driver for the General Standards PCI 16AIO board that demonstrates
// the creation of a driver that can be registered with the Linux kernel.  The Driver does the following:
// Detect the board by reading the PCI configuration space, remap the PCI BAR addresses to access the 
// PLX 9080 PCI interface chip and the local registers on the 16AIO, initialize the registers and finally
// perform a simple DMA transfer.
//
// This kernel driver does not implement calls needed for a user mode driver, it is not the intended scope 
// for this project.  Also, while interrupts shouldn't be hard to add, it was not added due to time 
// constraints.  A complete DMA solution would have interrupts, so that the CPU knows when DMA transfers 
// are completed.  Otherwise without it, you'd have to poll the device for completion status, making the
// advantages of DMA moot.
//
// The DMA transfer demonstrated here is a 32-bit one-shot transfer initiated by the CPU by setting a CMD 
// DMA register in the 16AIO's PLX PCI chip.  The DMA is a transfer from system memory to PCI memory.  
// First, a DMA buffer is allocated using "kmalloc" with the GFP_DMA flag, so that memory is from the DMA 
// zone.  Then, using pci_map_single(), we get the physical memory pointer to the DMA buffer we allocated
// in virtual memory.  With the pointer to the DMA's physical location, the PLX's DMA engine's registers 
// are configured to copy the contents of the system memory to an arbitrary register in the PCI card.  I 
// chose the card's Output Data Buffer Control Register (0x1C) since it's a 32-bit read/write register.  
//
// With the system configured for DMA, I set the DMA buffer to 0x2222 and send the command to start the 
// DMA transfer. The 16AIO PCI card becomes Bus Master and copies the system memory. I then read out the 
// 0x1C register to verify that the 0x2222 value was written to the card's memory.  
//
// Driver is written based on:
// 1) pmc16aio_man_063010.pdf User's Manual for the 16AIO Card
// 2) Broadcom PLX 9080 Data Book
// 3) Linux Device Drivers, Third Edition, by Jonathan Corbert, Alesssandro Rubini,
//		Greg Kroah-Hartman.  lwn.net/Kernel/LDD3
// 4) Dynamic DMA mapping Guide - https://www.kernel.org/doc/Documentation/DMA-API-HOWTO.txt


#define VENDOR_ID 0x10b5
#define DEVICE_ID 0x9080
#define SUBDEV_ID 0x2402

#define DEVICE_MAJOR 29
#define CLASS_NAME "16aio"
#define DEVICE_NAME "16aio"
#define NUM_MAX_ADAPTERS 4
#define PRINT_REG_32(s, r) printk(KERN_INFO DEVICE_NAME ": %30s 0x%02zx 0x%08x\n", #r, offsetof(s, r), (unsigned int) le32_to_cpu(regs->r))
#define PRINT_REG_16(s, r) printk(KERN_INFO DEVICE_NAME ": %30s 0x%02zx 0x%04x\n", #r, offsetof(s, r), (unsigned int) le16_to_cpu(regs->r))
#define PRINT_REG_08(s, r) printk(KERN_INFO DEVICE_NAME ": %30s 0x%02zx 0x%02x\n", #r, offsetof(s, r), (unsigned int) le16_to_cpu(regs->r))

//DMA Stuff
unsigned int * dma_vaddr;
dma_addr_t dma_paddr;

// these register defs come from the 16AIO User Manual.
// the manual calls them "GSC specific registers."  
typedef struct __attribute__ ((packed)) {
	u32 board_ctrl;     // 00
	u32 intr_ctrl;      // 04
	u32 inp_data_buf;   // 08  read-only
	u32 inp_buf_ctrl;   // 0c
	u32 rate_a_gen;     // 10
	u32 rate_b_gen;     // 14
	u32 out_data_buf;   // 18  write-only
	u32 out_buf_ctrl;   // 1c
	u32 scan_sync_ctrl; // 20
	u32 io_port;        // 24
	u32 fw_rev;         // 28  read-only
	u32 autocal;        // 2c
	u32 reserved[4];    // 30-3f
} gsc_cfg_regs_t;

// these are the PLX register definitions.  they are defined in "PCI 9080 Data Book".
typedef struct __attribute__ ((packed)) {
	u32 direct_slave_range;            	// 00
	u32 direct_slave_local_address;    	// 04
	u32 dma_arbitration;               	// 08
	u8 endian_desc;                    	// 0c
	u8 misc_control_1;
	u8 eeprom_write_protect_boundary;
	u8 misc_control_2;
	u32 slave_expansion_rom_address;   	// 10
	u32 slace_expansion_address;       	// 14
	u32 expansion_rom_descriptor;      	// 18
	u32 m2p_range;                     	// 1c
	u32 m2p_address;                   	// 20
	u32 m2p_config;                    	// 24
	u32 m2p_config_address;            	// 28
	u32 direct_space_address;          	// 2c
	u32 unused[4];                     	// 30
	u32 mailbox1[8];                   	// 40
	u32 p2l_doorbell;                  	// 60
	u32 l2p_doorbell;                  	// 64
	u32 int_ctrl;                      	// 68
	u32 ctrl;                          	// 6c
	u16 pci_vendor;                    	// 70
	u16 pci_device;
	u8 pci_revision;                   	// 74
	u8 rt_res[3];
	u32 mailbox2[2];                 	// 78
	u32 dma0mode;			   			// 80
	u32 dma0pciaddr;		   			// 84
	u32 dma0lcladdr;		   			// 88
	u32 dma0bytecnt;		   			// 8c
	u32 dma0pnt;			   			// 90
	u32 dma1mode;                      	// 94
    u32 dma1pciaddr;                   	// 98
    u32 dma1lcladdr;                   	// 9c
    u32 dma1bytecnt;                   	// a0
    u32 dma1pnt;                       	// a4
	u16 dmacmd;			   				// a8
	u16 poop;			   				// aa
	u32 dmaarb;			   				// ac
	u32 dmathreshold;		   			// b0

} plx_cfg_regs_t;


// struct for all the states we want from the kernel driver
typedef struct {
	struct pci_dev *dev;                   // dev provided by kernel
	int dev_minor;                         // device minor id
	volatile gsc_cfg_regs_t *gsc_regs;     // 16AIO registers
	volatile plx_cfg_regs_t *plx_regs;     // PLX registers
	int irq;                               // IRQ provided by kernel
} device_adapter_info_t;

static int device_proc_open(struct inode *inode, struct  file *file);
static int device_proc_show(struct seq_file *sfile, void *v);

static struct proc_dir_entry *proc_file = NULL;
static struct class *class  = NULL;
static unsigned int adapter_count = 0;
static device_adapter_info_t adapters[NUM_MAX_ADAPTERS];

// name for /proc/interrupts.
static const char intr_names[NUM_MAX_ADAPTERS][sizeof(DEVICE_NAME) + 2] = {
	DEVICE_NAME ".0",
	DEVICE_NAME ".1",
	DEVICE_NAME ".2",
	DEVICE_NAME ".3"
};

// standard kernel struct for an entry in procfs.  we have a proc file at
// /proc/16aio which gives info about the adapter in the system.  user space
// uses the info to latch on to the intended adapter.
static const struct file_operations proc_fops = {
	.owner = THIS_MODULE,
	.open = device_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

// standard kernel struct for kernel modules.
const struct file_operations fops = {
	.owner   = THIS_MODULE,
};

// procfs open.
static int
device_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, device_proc_show, NULL);
}

// procfs read.
static int
device_proc_show(struct seq_file *sfile, void *v)
{
	int i = 0;
	device_adapter_info_t *adapter = NULL;

	seq_printf(sfile, "id jumpers cpu pri device pci refs\n");

	for(i = 0; i < adapter_count; i++) {
		adapter = &adapters[i];
		seq_printf(
				sfile,
				"/dev/" DEVICE_NAME ".%d %04x:%02x:%02x.%d\n",
				i,
				pci_domain_nr(adapter->dev->bus),
				adapter->dev->bus->number,
				PCI_SLOT(adapter->dev->devfn),
				PCI_FUNC(adapter->dev->devfn));
	}

	return 0;
}

// print statements!
static void
device_print_gsc_regs(device_adapter_info_t *adapter)
{
	volatile gsc_cfg_regs_t *regs = adapter->gsc_regs;

	printk(KERN_INFO DEVICE_NAME ": %s\n", __FUNCTION__);

	PRINT_REG_32(gsc_cfg_regs_t, board_ctrl);
	PRINT_REG_32(gsc_cfg_regs_t, intr_ctrl);
	PRINT_REG_32(gsc_cfg_regs_t, inp_data_buf);
	PRINT_REG_32(gsc_cfg_regs_t, inp_buf_ctrl);
	PRINT_REG_32(gsc_cfg_regs_t, rate_a_gen);
	PRINT_REG_32(gsc_cfg_regs_t, rate_b_gen);
	PRINT_REG_32(gsc_cfg_regs_t, out_data_buf); //write-only
	PRINT_REG_32(gsc_cfg_regs_t, out_buf_ctrl);
	PRINT_REG_32(gsc_cfg_regs_t, scan_sync_ctrl);
	PRINT_REG_32(gsc_cfg_regs_t, io_port);
	PRINT_REG_32(gsc_cfg_regs_t, fw_rev);
	PRINT_REG_32(gsc_cfg_regs_t, autocal);
}

static void
device_print_plx_regs(device_adapter_info_t *adapter)
{
	volatile plx_cfg_regs_t *regs = adapter->plx_regs;

	printk(KERN_INFO DEVICE_NAME ": %s\n", __FUNCTION__);

	PRINT_REG_32(plx_cfg_regs_t, direct_slave_range);
	PRINT_REG_32(plx_cfg_regs_t, direct_slave_local_address);
	PRINT_REG_32(plx_cfg_regs_t, dma_arbitration);
	PRINT_REG_08(plx_cfg_regs_t, endian_desc);
	PRINT_REG_08(plx_cfg_regs_t, misc_control_1);
	PRINT_REG_08(plx_cfg_regs_t, eeprom_write_protect_boundary);
	PRINT_REG_08(plx_cfg_regs_t, misc_control_2);
	PRINT_REG_32(plx_cfg_regs_t, slave_expansion_rom_address);
	PRINT_REG_32(plx_cfg_regs_t, slace_expansion_address);
	PRINT_REG_32(plx_cfg_regs_t, expansion_rom_descriptor);
	PRINT_REG_32(plx_cfg_regs_t, m2p_range);
	PRINT_REG_32(plx_cfg_regs_t, m2p_address);
	PRINT_REG_32(plx_cfg_regs_t, m2p_config);
	PRINT_REG_32(plx_cfg_regs_t, m2p_config_address);
	PRINT_REG_32(plx_cfg_regs_t, direct_space_address);
	PRINT_REG_32(plx_cfg_regs_t, p2l_doorbell);
	PRINT_REG_32(plx_cfg_regs_t, l2p_doorbell);
	PRINT_REG_32(plx_cfg_regs_t, int_ctrl);
	PRINT_REG_16(plx_cfg_regs_t, pci_vendor);
	PRINT_REG_16(plx_cfg_regs_t, pci_device);
	PRINT_REG_08(plx_cfg_regs_t, pci_revision);

	PRINT_REG_32(plx_cfg_regs_t, dma0mode);
	PRINT_REG_32(plx_cfg_regs_t, dma0pciaddr);
	PRINT_REG_32(plx_cfg_regs_t, dma0lcladdr);
	PRINT_REG_32(plx_cfg_regs_t, dma0bytecnt);
	PRINT_REG_32(plx_cfg_regs_t, dma0pnt);
	PRINT_REG_32(plx_cfg_regs_t, dma1mode);
	PRINT_REG_32(plx_cfg_regs_t, dma1pciaddr);
    PRINT_REG_32(plx_cfg_regs_t, dma1lcladdr);
    PRINT_REG_32(plx_cfg_regs_t, dma1bytecnt);
    PRINT_REG_32(plx_cfg_regs_t, dma1pnt);
	PRINT_REG_16(plx_cfg_regs_t, dmacmd);
	PRINT_REG_16(plx_cfg_regs_t, poop);
	PRINT_REG_32(plx_cfg_regs_t, dmaarb);
	PRINT_REG_32(plx_cfg_regs_t, dmathreshold);

}

// at init time, we want to reset the board so it's in a known state.
static void
device_reset_board(device_adapter_info_t *adapter)
{
	u32 val = 0;

	adapter->gsc_regs->board_ctrl = cpu_to_le32((u32) 0x00008000);

	do {
		cond_resched();
		val = 0x0;  // todo
		val = le32_to_cpu( adapter->gsc_regs->board_ctrl );
	} while (val & 0x00008000);
}

// disable the PLX from notifying the kernel about interrupts from this
// adapter.  
static void
device_disable_plx_interrupts(device_adapter_info_t *adapter)
{
	u32 val = adapter->plx_regs->int_ctrl;

	val = le32_to_cpu(val) & ~(1 << 11);
	adapter->plx_regs->int_ctrl = cpu_to_le32(val);
}


// our kernel module entry point.
static int
__init device_init(struct pci_dev *dev, const struct pci_device_id *id)
{
	device_adapter_info_t *adapter = NULL;

	printk(KERN_INFO DEVICE_NAME ": device_init %p %02x:%02x.%d.\n",
			dev, dev->bus->number, PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn));

	if (adapter_count == 0) {
		// create /proc/16aio
		proc_file = proc_create(DEVICE_NAME, 0, NULL, &proc_fops);

		// create a class for our character devices
		register_chrdev(DEVICE_MAJOR, DEVICE_NAME, &fops);
		class = class_create(THIS_MODULE, CLASS_NAME);
	}

	printk(KERN_INFO DEVICE_NAME ": Dev %d will request irq %d %p\n", adapter_count, dev->irq, dev);

	adapter = &adapters[adapter_count];

	if (pci_request_regions(dev, "GS 16AIO driver - !!") != 0) {
		printk(KERN_INFO DEVICE_NAME ": pci_request_regions failed\n");
	}

	adapter->dev = dev;
	adapter->dev_minor = adapter_count + 1;
	adapter->plx_regs = (plx_cfg_regs_t *) pci_ioremap_bar(dev, 0);
	adapter->gsc_regs = (gsc_cfg_regs_t *) pci_ioremap_bar(dev, 2);
	adapter->irq = dev->irq;

	device_reset_board(adapter);
	device_disable_plx_interrupts(adapter);

	if (pci_enable_device(adapter->dev)) {
		printk(KERN_INFO DEVICE_NAME ": pci_enable_device failed\n");
	}
	
	/************************/
	/* DMA TESTCODE IS HERE */
	/************************/
	
	//Set these registers to a unique value using CPU I/O first.
	adapter->gsc_regs->out_buf_ctrl = cpu_to_le32(0xAA);
    adapter->plx_regs->dma0mode =  cpu_to_le32(0x01);
	device_print_gsc_regs(adapter);
	device_print_plx_regs(adapter);

	//enable PCI Device Bus Master
	pci_set_master(adapter->dev);
	if(pci_set_dma_mask(adapter->dev, DMA_BIT_MASK(32))) {
		printk(KERN_ALERT "DMA NOT SUPPORTED!\n");
	}
	else printk(KERN_ALERT "DMA WORKS! YEAH!\n");


	//Allocate System DMA memory and find physical pointer to it
	dma_vaddr = kmalloc(32, GFP_KERNEL | GFP_DMA);
	memset(dma_vaddr, 0x22, 4);
	dma_paddr = pci_map_single(adapter->dev, dma_vaddr, 4, PCI_DMA_TODEVICE);

	// Clear interrupts
	adapter->plx_regs->dmacmd = cpu_to_le16(0x08);
	adapter->plx_regs->dmacmd = cpu_to_le16(0x00);

	// Setup DMA Engine on 16AIO board
	adapter->plx_regs->dma0mode = cpu_to_le32(0x00020D43); //32-bit transfers
	adapter->plx_regs->dma0pciaddr = cpu_to_le32(dma_paddr);
	adapter->plx_regs->dma0lcladdr = cpu_to_le32(0x1C);	//Output Data Buffer Control Reg
	adapter->plx_regs->dma0bytecnt = cpu_to_le32(0x4);	//4-bytes
	adapter->plx_regs->dma0pnt = cpu_to_le32(0x0); 	//PCI bus to Local Bus Transfer
	
	// DMA Transfer Happens Here!
	adapter->plx_regs->dmacmd = cpu_to_le16(0x1);	//Enable DMA
	adapter->plx_regs->dmacmd = cpu_to_le16(0x3);	//Initiate DMA transfer!

	printk(KERN_ALERT "DMA Transferred Initiated!\n");

	device_print_gsc_regs(adapter);


	// create /dev/16aio.<num>
	device_create(
			class,
			NULL,
			MKDEV(DEVICE_MAJOR, adapter->dev_minor),
			NULL,
			DEVICE_NAME ".%d",
			adapter_count);

	adapter_count++;

	return 0;
}

// our kernel module exit point.
static void
__exit device_exit(struct pci_dev *dev)
{
	device_adapter_info_t *adapter = NULL;
	int i = 0;

	printk(KERN_INFO DEVICE_NAME ": device_exit.\n");

	for (i = 0; i < adapter_count; i++) {
		if (adapters[i].dev == dev) {
			adapter = &adapters[i];
			break;
		}
	}

	printk(KERN_INFO DEVICE_NAME ": destroying %p.\n", adapter);

	device_disable_plx_interrupts(adapter);

	free_irq(adapter->irq, adapter);
	device_destroy(class, MKDEV(DEVICE_MAJOR, adapter->dev_minor));
	pci_release_regions(adapter->dev);
	pci_disable_device(adapter->dev);
	memset(adapter, 0, sizeof(device_adapter_info_t));

	adapter_count--;

	if (adapter_count == 0) {
		printk(KERN_INFO DEVICE_NAME ": destroyed all adapters.\n");
		class_unregister(class); 
		class_destroy(class); 
		unregister_chrdev(DEVICE_MAJOR, DEVICE_NAME);

		remove_proc_entry(DEVICE_NAME, NULL);
	}
}

// standard struct for linux kernel; what PCI devices this driver handles.
static struct pci_device_id device_pci_ids[] = {
	{
		.vendor = VENDOR_ID,
		.device = DEVICE_ID,
		.subvendor = VENDOR_ID,
		.subdevice = SUBDEV_ID,
	},
	{0, 0, 0, 0}
};

// standard struct for linux kernel.
static struct pci_driver device_pci_driver_template = {
	.name = "GS 16AIO PCI driver",
	.id_table = device_pci_ids,
	.probe = device_init,
	.remove = device_exit,
};


module_pci_driver(device_pci_driver_template);
MODULE_DEVICE_TABLE(pci, device_pci_ids);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("John Quach");
