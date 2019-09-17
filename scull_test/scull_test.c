#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/kernel.h> /* printk */
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/slab.h> /* kmalloc */

#include "scull_test.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("mindentropy");

int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devices = SCULL_NR_DEVICES;

struct scull_dev *scull_devices = NULL;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devices, int, S_IRUGO);
/*module_param(scull_quantum, int, S_IRUGO);
module_param(scull_qset, int, S_IRUGO);*/

int scull_open(struct inode *inode, struct file *filp)
{
	return 0;
}

int scull_release(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t scull_read(
				struct file *filp,
				char __user *buff,
				size_t count,
				loff_t *f_pos
			)
{
	return 0;
}

ssize_t scull_write(
			struct file *filp,
			const char __user *buff,
			size_t count,
			loff_t *f_pos
			)
{
	return 0;
}

loff_t scull_llseek(
		struct file *filp,
		loff_t off,
		int whence
		)
{
	return 0;
}

long scull_ioctl(
		struct file *filp,
		unsigned int cmd,
		unsigned long arg
		) {
	return 0;
}

struct file_operations scull_fops = {
	.owner = THIS_MODULE,
	.open = scull_open,
	.release = scull_release,
	.read = scull_read,
	.write = scull_write,
	.llseek = scull_llseek,
	.unlocked_ioctl = scull_ioctl,
	.compat_ioctl = scull_ioctl,
};

static void scull_cleanup_module(void)
{
	dev_t dev = MKDEV(scull_major, scull_minor);
	unregister_chrdev_region(dev, scull_nr_devices);

	printk(KERN_DEBUG "%s\n", __func__);
}

static int __init scull_init_module(void)
{
	dev_t dev = 0;
	int result = 0, i = 0;

	if(scull_major) {
		printk(KERN_DEBUG "manual allocation: scull major defined\n");
		dev = MKDEV(scull_major, scull_minor);
		result = register_chrdev_region(dev, scull_nr_devices, "scull");
	} else {
		printk(KERN_DEBUG "Dynamic allocation\n");
		result = alloc_chrdev_region(&dev, scull_minor, scull_nr_devices, "scull");
		scull_major = MAJOR(dev);
		printk(KERN_DEBUG "Allocated major number: %d\n", scull_major);
	}

	if(result < 0) {
		printk(KERN_DEBUG "scull: can't get major %d\n", scull_major);
		return result;
	}

	scull_devices = kmalloc(
						sizeof(struct scull_dev) * scull_nr_devices,
						GFP_KERNEL
					);

	if(!scull_devices) {
		printk(KERN_DEBUG "Could not allocate memory\n");
		result = -ENOMEM;
		goto fail;
	}
	
	memset(scull_devices, 0, sizeof(struct scull_dev) * scull_nr_devices);

	for(i = 0; i<scull_nr_devices; i++)
	{
/*		scull_devices[i].sem = */
/*		scull_devices[i].cdev = scull_cdev_init(*/
	}

fail:
	scull_cleanup_module();
	return result;
}


module_init(scull_init_module);
module_exit(scull_cleanup_module);
