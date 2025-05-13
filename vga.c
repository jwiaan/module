#include <linux/module.h>
#include <asm/io.h>

MODULE_LICENSE("GPL");

unsigned int major;
char *vga, *end;

static int open(struct inode *, struct file *)
{
	return 0;
}

static int release(struct inode *, struct file *)
{
	return 0;
}

static ssize_t write(struct file *, const char __user *u, size_t n, loff_t *l)
{
	char *p = vga + *l;
	if (p + n > end)
		n = end - p;

	char *k = kmalloc(n, GFP_KERNEL);
	if (copy_from_user(k, u, n)) {
		kfree(k);
		return -EFAULT;
	}

	memcpy_toio(p, k, n);
	*l += n;
	kfree(k);
	return n;
}

struct file_operations fo = {
	.owner = THIS_MODULE,
	.open = open,
	.release = release,
	.write = write,
};

static int vga_init(void)
{
	major = register_chrdev(0, "vga", &fo);
	size_t n = 0xC0000 - 0xb8000;
	vga = ioremap(0xb8000, n);
	end = vga + n;
	printk(KERN_ALERT "%s %u %p\n", __func__, major, vga);
	return 0;
}

static void vga_exit(void)
{
	iounmap(vga);
	unregister_chrdev(major, "vga");
}

module_init(vga_init);
module_exit(vga_exit);
