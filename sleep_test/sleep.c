#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/wait.h>

MODULE_LICENSE("Dual BSD/GPL");

static int sleepy_major = 0;
static wait_queue_head_t wq;
static int flag = 0;


ssize_t sleepy_read(struct file *filp, char __user *buff, size_t count, loff_t *pos)
{
	printk(KERN_DEBUG "process %i (%s) going to sleep \n",
					current->pid,
					current->comm);

	wait_event_interruptible(wq, flag != 0);

	flag = 0;
	printk(KERN_DEBUG "awoken %i (%s) \n", current->pid, current->comm);

	return 0;
}

ssize_t sleepy_write(struct file *filp, const char __user *buff, size_t count, loff_t *pos)
{
	printk(KERN_DEBUG "process %i (%s) awakening readers ...\n",
							current->pid, current->comm);
	flag = 1;
	wake_up_interruptible(&wq);
	return count;
}

struct file_operations sleep_fops = {
	.owner = THIS_MODULE,
	.read = sleepy_read,
	.write = sleepy_write,
};

int sleepy_init(void)
{
	int retval = 0;
	dev_t dev;

	retval = alloc_chrdev_region(&dev, 0, 1, "sleepy");
	
	if(retval < 0) {
		return retval;
	} else {
		sleepy_major = MAJOR(dev);
		init_waitqueue_head(&wq);
	}

	return 0;
}

void sleepy_exit(void)
{
	unregister_chrdev(sleepy_major, "sleepy");
}

module_init(sleepy_init);
module_exit(sleepy_exit);
