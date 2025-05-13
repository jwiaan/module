#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/list.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#define N 10

struct test {
	uid_t uid;
	char buf[N];
	size_t get, put;
	struct mutex mutex;
	wait_queue_head_t rq, wq;
	struct list_head list;
};

MODULE_LICENSE("GPL");
LIST_HEAD(list);
DEFINE_MUTEX(mutex);
dev_t dev;
struct cdev cdev;
char *name = "test";
module_param(name, charp, S_IRUGO);

static bool empty(const struct test *t)
{
	return t->get == t->put;
}

static bool full(const struct test *t)
{
	return t->put - t->get == N;
}

static struct test *find(uid_t uid)
{
	struct test *t;
	list_for_each_entry(t, &list, list) {
		if (t->uid == uid)
			return t;
	}

	t = kmalloc(sizeof(*t), GFP_KERNEL);
	t->uid = uid;
	t->get = t->put = 0;
	mutex_init(&t->mutex);
	init_waitqueue_head(&t->rq);
	init_waitqueue_head(&t->wq);
	list_add(&t->list, &list);
	return t;
}

static int open(struct inode *i, struct file *f)
{
	if (mutex_lock_interruptible(&mutex))
		return -ERESTARTSYS;

	f->private_data = find(current_uid().val);
	mutex_unlock(&mutex);
	return nonseekable_open(i, f);
}

static int release(struct inode *, struct file *)
{
	return 0;
}

static ssize_t read(struct file *f, char __user *c, size_t s, loff_t *)
{
	struct test *t = f->private_data;
	if (mutex_lock_interruptible(&t->mutex))
		return -ERESTARTSYS;

	while (empty(t)) {
		mutex_unlock(&t->mutex);
		if (f->f_flags & O_NONBLOCK)
			return -EAGAIN;

		if (wait_event_interruptible(t->rq, !empty(t)))
			return -ERESTARTSYS;

		if (mutex_lock_interruptible(&t->mutex))
			return -ERESTARTSYS;
	}

	size_t p = t->get % N;
	size_t n = min3(t->put - t->get, N - p, s);
	if (copy_to_user(c, &t->buf[p], n)) {
		mutex_unlock(&t->mutex);
		return -EFAULT;
	}

	t->get += n;
	mutex_unlock(&t->mutex);
	wake_up_interruptible(&t->wq);
	return n;
}

static ssize_t write(struct file *f, const char __user *c, size_t s, loff_t *)
{
	struct test *t = f->private_data;
	if (mutex_lock_interruptible(&t->mutex))
		return -ERESTARTSYS;

	while (full(t)) {
		mutex_unlock(&t->mutex);
		if (f->f_flags & O_NONBLOCK)
			return -EAGAIN;

		if (wait_event_interruptible(t->wq, !full(t)))
			return -ERESTARTSYS;

		if (mutex_lock_interruptible(&t->mutex))
			return -ERESTARTSYS;
	}

	size_t p = t->put % N;
	size_t n = min3(N - (t->put - t->get), N - p, s);
	if (copy_from_user(&t->buf[p], c, n)) {
		mutex_unlock(&t->mutex);
		return -EFAULT;
	}

	t->put += n;
	mutex_unlock(&t->mutex);
	wake_up_interruptible(&t->rq);
	return n;
}

static unsigned poll(struct file *f, poll_table *pt)
{
	struct test *t = f->private_data;
	unsigned u = 0;
	mutex_lock(&t->mutex);
	poll_wait(f, &t->rq, pt);
	poll_wait(f, &t->wq, pt);
	if (!empty(t))
		u |= POLLIN | POLLRDNORM;

	if (!full(t))
		u |= POLLOUT | POLLWRNORM;

	mutex_unlock(&t->mutex);
	return u;
}

struct file_operations fo = {
	.owner = THIS_MODULE,
	.open = open,
	.release = release,
	.read = read,
	.write = write,
	.poll = poll,
	.llseek = no_llseek,
};

static int show(struct seq_file *s, void *)
{
	if (mutex_lock_interruptible(&mutex))
		return -ERESTARTSYS;

	struct test *t;
	list_for_each_entry(t, &list, list) {
		seq_printf(s, "%d: get=%lu put=%lu\n", t->uid, t->get, t->put);
		for (int i = 0; i < N; ++i)
			seq_putc(s, t->buf[i]);

		seq_putc(s, '\n');
	}

	mutex_unlock(&mutex);
	return 0;
}

static int proc_open(struct inode *, struct file *f)
{
	return single_open(f, show, NULL);
}

struct proc_ops po = {
	.proc_open = proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release
};

static int test_init(void)
{
	alloc_chrdev_region(&dev, 0, 1, name);
	cdev_init(&cdev, &fo);
	cdev.owner = THIS_MODULE;
	cdev_add(&cdev, dev, 1);
	proc_create(name, 0, NULL, &po);
	printk(KERN_ALERT "%s: %s %d\n", __func__, name, MAJOR(dev));
	return 0;
}

static void test_exit(void)
{
	struct test *t, *next;
	list_for_each_entry_safe(t, next, &list, list) {
		list_del(&t->list);
		kfree(t);
	}

	remove_proc_entry(name, NULL);
	cdev_del(&cdev);
	unregister_chrdev_region(dev, 1);
	printk(KERN_ALERT "%s: %s %d\n", __func__, name, MAJOR(dev));
}

module_init(test_init);
module_exit(test_exit);
