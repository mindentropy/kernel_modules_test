#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/kernel.h> /* printk */
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/slab.h> /* kmalloc */
#include <linux/semaphore.h>
#include <linux/uaccess.h>

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
	struct scull_dev *dev;

	printk(KERN_DEBUG "Minor number, device to be opened: %d\n", MINOR(inode->i_rdev));
	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	filp->private_data = dev; /* filp -> Pointer to a open file, which will be passed around */

	if( (filp->f_flags & O_ACCMODE)  == O_WRONLY ) {
		if ( down_interruptible(&dev->sem) ) {
			return -ERESTARTSYS; /*
									Restart the system call transparently by the kernel itself
									if the process is interrupted by the signal
								*/
		}
		/* Trim the storage */

		up(&dev->sem);
	}

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
	ssize_t retval = -ENOMEM;
	struct scull_dev *scull_dev = filp->private_data;
	int i = 0;

	char tmpbuff[2];

	if(down_interruptible(&(scull_dev->sem))) {
		return -ERESTARTSYS;
	}


	if (count > 2) {
		count = 2;
	}
	if(copy_from_user(tmpbuff, buff, count)) {
		retval = -EFAULT;
		goto out;
	}

	for(i = 0; i<count; i++)
	{
		printk(KERN_DEBUG "%c", tmpbuff[i]);
	}

	retval = count;

	out:
		up(&(scull_dev->sem));
		return retval;
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
	int i;
	dev_t dev = MKDEV(scull_major, scull_minor);

	if(scull_devices) {
		for(i = 0; i<scull_nr_devices; i++) {
			cdev_del(&(scull_devices[i].cdev));
		}
		kfree(scull_devices);
	}

	unregister_chrdev_region(dev, scull_nr_devices);

	printk(KERN_DEBUG "%s\n", __func__);
}

static void scull_setup_cdev(struct scull_dev *scull_dev, int index)
{
	int err;
	dev_t devno = MKDEV(scull_major, scull_minor + index);

	cdev_init(&(scull_dev->cdev), &scull_fops);
	scull_dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&(scull_dev->cdev), devno, 1);

	if(err) {
		printk(KERN_NOTICE "Error %d adding scull %d\n", err, index);
	}
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
		sema_init(&(scull_devices[i].sem), 1);
		scull_setup_cdev(&(scull_devices[i]), i);
	}

	return 0;

fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
