//#include <linux/kernel.h>
#include <linux/module.h>
//#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lyramilk");
MODULE_DESCRIPTION("wasm");

static int __init lywr_init(void)
{
      printk(KERN_WARNING "lywr_init!\n");
      return 0;
}
static void __exit lywr_exit(void)
{
      printk(KERN_WARNING "lywr_exit!\n");
}

module_init(lywr_init);
module_exit(lywr_exit);
