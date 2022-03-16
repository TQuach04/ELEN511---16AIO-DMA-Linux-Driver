#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xac8c0396, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x7fb7cce2, __VMLINUX_SYMBOL_STR(single_release) },
	{ 0xf0c25ce4, __VMLINUX_SYMBOL_STR(seq_read) },
	{ 0x1a90fea4, __VMLINUX_SYMBOL_STR(seq_lseek) },
	{ 0x3ff1d33, __VMLINUX_SYMBOL_STR(pci_unregister_driver) },
	{ 0x4dc278c4, __VMLINUX_SYMBOL_STR(__pci_register_driver) },
	{ 0xe337e8d9, __VMLINUX_SYMBOL_STR(device_create) },
	{ 0x48f73464, __VMLINUX_SYMBOL_STR(mem_map) },
	{ 0x12da5bb2, __VMLINUX_SYMBOL_STR(__kmalloc) },
	{ 0xab5666a3, __VMLINUX_SYMBOL_STR(dma_set_mask) },
	{ 0xa19f5133, __VMLINUX_SYMBOL_STR(pci_set_master) },
	{ 0x6ff1b0ee, __VMLINUX_SYMBOL_STR(pci_enable_device) },
	{ 0xa1c76e0a, __VMLINUX_SYMBOL_STR(_cond_resched) },
	{ 0x3670eaa5, __VMLINUX_SYMBOL_STR(pci_ioremap_bar) },
	{ 0xbeb4cce0, __VMLINUX_SYMBOL_STR(pci_request_regions) },
	{ 0x165b4598, __VMLINUX_SYMBOL_STR(__class_create) },
	{ 0x82147978, __VMLINUX_SYMBOL_STR(__register_chrdev) },
	{ 0x28c3e78e, __VMLINUX_SYMBOL_STR(proc_create_data) },
	{ 0x5bd32f2b, __VMLINUX_SYMBOL_STR(remove_proc_entry) },
	{ 0x6bc3fbc0, __VMLINUX_SYMBOL_STR(__unregister_chrdev) },
	{ 0x892114e4, __VMLINUX_SYMBOL_STR(class_destroy) },
	{ 0x28b96477, __VMLINUX_SYMBOL_STR(class_unregister) },
	{ 0xb7e24b6e, __VMLINUX_SYMBOL_STR(pci_disable_device) },
	{ 0xe3f75699, __VMLINUX_SYMBOL_STR(pci_release_regions) },
	{ 0x576bf97c, __VMLINUX_SYMBOL_STR(device_destroy) },
	{ 0xf20dabd8, __VMLINUX_SYMBOL_STR(free_irq) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x42f55f52, __VMLINUX_SYMBOL_STR(pci_domain_nr) },
	{ 0x5cb8a6c3, __VMLINUX_SYMBOL_STR(seq_printf) },
	{ 0x88453668, __VMLINUX_SYMBOL_STR(single_open) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("pci:v000010B5d00009080sv000010B5sd00002402bc*sc*i*");
