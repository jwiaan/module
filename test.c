#include <linux/module.h>
#include <linux/cdev.h>

#define SIZE 10

MODULE_LICENSE("GPL");

char *name = "wang";
module_param(name, charp, S_IRUGO);

struct test {
	dev_t dev;
	char buf[SIZE];
	struct cdev cdev;
} test;

static int test_open(struct inode *i, struct file *f)
{
	struct test *t = container_of(i->i_cdev, struct test, cdev);
	f->private_data = t;
	return 0;
}

static ssize_t test_read(struct file *f, char __user *c, size_t s, loff_t *l)
{
	struct test *t = f->private_data;
	if (*l >= SIZE)
		return 0;

	if (*l + s > SIZE)
		s = SIZE - *l;

	if (copy_to_user(c, &t->buf[*l], s))
		return -1;

	*l += s;
	return s;
}

static ssize_t test_write(struct file *f, const char __user *c, size_t s,
			  loff_t *l)
{
	struct test *t = f->private_data;
	if (*l >= SIZE)
		return -5;

	if (*l + s > SIZE)
		s = SIZE - *l;

	if (copy_from_user(&t->buf[*l], c, s))
		return -1;

	*l += s;
	return s;
}

static int test_release(struct inode *i, struct file *f)
{
	return 0;
}

struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = test_open,
	.read = test_read,
	.write = test_write,
	.release = test_release,
};

static int test_init(void)
{
	alloc_chrdev_region(&test.dev, 3, 1, name);
	cdev_init(&test.cdev, &fops);
	test.cdev.owner = THIS_MODULE;
	cdev_add(&test.cdev, test.dev, 1);
	printk(KERN_ALERT "%s %s %d %d\n", __func__, name, MAJOR(test.dev),
	       MINOR(test.dev));
	return 0;
}

static void test_exit(void)
{
	cdev_del(&test.cdev);
	unregister_chrdev_region(test.dev, 1);
	printk(KERN_ALERT "%s\n", __func__);
}

module_init(test_init);
module_exit(test_exit);
