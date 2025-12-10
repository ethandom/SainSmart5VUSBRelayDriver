#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

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



static const char ____versions[]
__used __section("__versions") =
	"\x10\x00\x00\x00\x7e\x3a\x2c\x12"
	"_printk\0"
	"\x18\x00\x00\x00\xeb\x07\x68\x42"
	"device_destroy\0\0"
	"\x14\x00\x00\x00\xaa\xdc\x01\x6b"
	"cdev_del\0\0\0\0"
	"\x14\x00\x00\x00\x14\xc5\xb7\xff"
	"ida_free\0\0\0\0"
	"\x14\x00\x00\x00\xfb\xc7\x80\xcf"
	"usb_put_dev\0"
	"\x10\x00\x00\x00\xba\x0c\x7a\x03"
	"kfree\0\0\0"
	"\x1c\x00\x00\x00\x2b\x2f\xec\xe3"
	"alloc_chrdev_region\0"
	"\x18\x00\x00\x00\xd9\x71\x85\x06"
	"class_create\0\0\0\0"
	"\x1c\x00\x00\x00\xf7\x3d\x8d\x9e"
	"usb_register_driver\0"
	"\x18\x00\x00\x00\x5d\x01\xd1\x0a"
	"class_destroy\0\0\0"
	"\x24\x00\x00\x00\x33\xb3\x91\x60"
	"unregister_chrdev_region\0\0\0\0"
	"\x18\x00\x00\x00\xde\x59\xa7\x6d"
	"usb_deregister\0\0"
	"\x18\x00\x00\x00\x2d\xca\xb7\x59"
	"usb_bulk_msg\0\0\0\0"
	"\x1c\x00\x00\x00\xcb\xf6\xfd\xf0"
	"__stack_chk_fail\0\0\0\0"
	"\x1c\x00\x00\x00\x63\xa5\x03\x4c"
	"random_kmalloc_seed\0"
	"\x18\x00\x00\x00\x59\x3f\xa5\xcf"
	"kmalloc_caches\0\0"
	"\x18\x00\x00\x00\x82\x76\xf2\xd1"
	"kmalloc_trace\0\0\0"
	"\x14\x00\x00\x00\xb0\x9e\xcb\x7e"
	"usb_get_dev\0"
	"\x18\x00\x00\x00\x9f\x0c\xfb\xce"
	"__mutex_init\0\0\0\0"
	"\x18\x00\x00\x00\x73\x25\xa0\xe7"
	"ida_alloc_range\0"
	"\x14\x00\x00\x00\x45\x0a\x98\x48"
	"cdev_init\0\0\0"
	"\x14\x00\x00\x00\x87\x16\x76\x0c"
	"cdev_add\0\0\0\0"
	"\x18\x00\x00\x00\xd5\xa5\x42\x59"
	"device_create\0\0\0"
	"\x18\x00\x00\x00\x67\x91\x07\x7f"
	"usb_control_msg\0"
	"\x20\x00\x00\x00\x28\xe1\xa4\x12"
	"__arch_copy_from_user\0\0\0"
	"\x14\x00\x00\x00\x4b\x8d\xfa\x4d"
	"mutex_lock\0\0"
	"\x18\x00\x00\x00\x38\xf0\x13\x32"
	"mutex_unlock\0\0\0\0"
	"\x24\x00\x00\x00\x52\x3f\x0a\x4b"
	"gic_nonsecure_priorities\0\0\0\0"
	"\x1c\x00\x00\x00\xef\x6d\x5c\xa6"
	"alt_cb_patch_nops\0\0\0"
	"\x18\x00\x00\x00\x8c\x89\xd4\xcb"
	"fortify_panic\0\0\0"
	"\x1c\x00\x00\x00\x54\xfc\xbb\x6c"
	"__arch_copy_to_user\0"
	"\x18\x00\x00\x00\xd4\xce\x9f\xb7"
	"module_layout\0\0\0"
	"\x00\x00\x00\x00\x00\x00\x00\x00";

MODULE_INFO(depends, "");

MODULE_ALIAS("usb:v0403p6001d*dc*dsc*dp*ic*isc*ip*in*");

MODULE_INFO(srcversion, "BAA72C65527719FBDC101BC");
