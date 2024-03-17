#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x4155277e, "module_layout" },
	{ 0x3dde6a5e, "platform_driver_unregister" },
	{ 0x77d2b6f1, "class_destroy" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x2d9dc5d0, "__platform_driver_register" },
	{ 0xfe736308, "__class_create" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x7e889eb3, "device_destroy" },
	{ 0x5bbe49f4, "__init_waitqueue_head" },
	{ 0xecb3276d, "cdev_del" },
	{ 0x3371d073, "device_create" },
	{ 0x8d23c1d7, "cdev_add" },
	{ 0x5431f8d2, "cdev_init" },
	{ 0xd9e26360, "of_node_put" },
	{ 0xaa239b8b, "of_find_node_by_phandle" },
	{ 0xf5a17887, "of_get_property" },
	{ 0x8da99d05, "devm_ioremap_resource" },
	{ 0x16045e94, "platform_get_resource" },
	{ 0xc5850110, "printk" },
	{ 0x822137e2, "arm_heavy_mb" },
	{ 0x624e8bd2, "devm_request_threaded_irq" },
	{ 0x5f754e5a, "memset" },
	{ 0x514cc273, "arm_copy_from_user" },
	{ 0x8de735df, "of_property_read_variable_u32_array" },
	{ 0xfc65a9c7, "devm_kmalloc" },
	{ 0x2cfde9a2, "warn_slowpath_fmt" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
};

MODULE_INFO(depends, "");

