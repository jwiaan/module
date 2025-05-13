#include <linux/cdev.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#define N 10

MODULE_LICENSE("GPL");

char *name = "wang";
module_param(name, charp, S_IRUGO);

struct test {
	dev_t dev;
	struct cdev cdev;
	char buf[N];
	size_t get, put;
} test;

static void *test_seq_start(struct seq_file *s, loff_t *l)
{
	if (*l > 0)
		return NULL;

	return &test;
}

static void *test_seq_next(struct seq_file *s, void *v, loff_t *l)
{
	++(*l);
	return NULL;
}

static void test_seq_stop(struct seq_file *s, void *v)
{
}

static int test_seq_show(struct seq_file *s, void *v)
{
	seq_printf(s, "major=%d minor=%d get=%lu put=%lu\n", MAJOR(test.dev),
		   MINOR(test.dev), test.get, test.put);
	return 0;
}

static struct seq_operations test_seq_ops = {
	.start = test_seq_start,
	.next = test_seq_next,
	.stop = test_seq_stop,
	.show = test_seq_show
};

static int test_seq_open(struct inode *i, struct file *f)
{
	return seq_open(f, &test_seq_ops);
}

static struct proc_ops test_proc_ops = {
	.proc_open = test_seq_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release
};

static int test_open(struct inode *i, struct file *f)
{
	struct test *t = container_of(i->i_cdev, struct test, cdev);
	f->private_data = t;
	return 0;
}

static ssize_t test_read(struct file *f, char __user *c, size_t s, loff_t *l)
{
	struct test *t = f->private_data;
	if (t->get >= t->put)
		return 0;

	size_t p = t->get % N;
	size_t n = min3(t->put - t->get, N - p, s);
	if (copy_to_user(c, &t->buf[p], n))
		return -1;

	t->get += n;
	return n;
}

static ssize_t test_write(struct file *f, const char __user *c, size_t s,
			  loff_t *l)
{
	struct test *t = f->private_data;
	size_t p = t->put % N;
	size_t n = N - p;
	n = n < s ? n : s;
	if (copy_from_user(&t->buf[p], c, n))
		return -1;

	t->put += n;
	return n;
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
	proc_create("testSeq", 0, NULL, &test_proc_ops);

	printk(KERN_ALERT "%s %s %d %d\n", __func__, name, MAJOR(test.dev),
	       MINOR(test.dev));
	return 0;
}

static void test_exit(void)
{
	cdev_del(&test.cdev);
	unregister_chrdev_region(test.dev, 1);
	remove_proc_entry("testSeq", NULL);
	printk(KERN_ALERT "%s\n", __func__);
}

module_init(test_init);
module_exit(test_exit);
