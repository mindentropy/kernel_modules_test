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
#include <linux/device.h>

#include "scull_test.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("mindentropy");

int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devices = SCULL_NR_DEVICES;
int scull_quantum = SCULL_QUANTUM;
int scull_qset = SCULL_QSET;

struct scull_dev *scull_devices = NULL;
static struct class *scull_class = NULL;
static struct device *scull_devs[SCULL_NR_DEVICES] = { NULL };

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devices, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);
module_param(scull_qset, int, S_IRUGO);

int scull_trim(struct scull_dev *dev)
{
	struct scull_qset *next, *dptr;
	int qset = dev->qset;
	int i;

	for(dptr = dev->data; dptr; dptr = next) {
		if(dptr->data) {
			for(i = 0; i<qset; i++) {
				kfree(dptr->data[i]);
			}
			kfree(dptr->data);
			dptr->data = NULL;
		}
		next = dptr->next; /* Go to the next node */
		kfree(dptr); /* Delete the node */
	}

	dev->size = 0;
	dev->quantum = scull_quantum;
	dev->qset = scull_qset;
	dev->data = NULL;

	return 0;
}

struct scull_qset * scull_follow(
							struct scull_dev *dev,
							int n
							)
{
	struct scull_qset *qset = dev->data;

	if(!qset) { /* If the node is empty. Allocate and return */
		qset = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
		if(qset == NULL)
			return NULL;

		memset(qset, 0, sizeof(struct scull_qset));
	}

	/* Follow the list */
	while(n--) {
		if(qset->next == NULL) {
			/* Allocate as you go */
			qset->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
			if(qset->next == NULL) {
				return NULL;
			}
			memset(qset, 0, sizeof(struct scull_qset));
		}
		qset = qset->next;
	}

	return qset;
}


int scull_open(struct inode *inode, struct file *filp)
{
	struct scull_dev *dev;

	printk(KERN_DEBUG "Minor number, device to be opened: %d\n", MINOR(inode->i_rdev));
	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	filp->private_data = dev; /* filp -> Pointer to a open file, which will be passed around */

	if((filp->f_flags & O_ACCMODE)  == O_WRONLY ) {
		if (down_interruptible(&dev->sem)) {
			return -ERESTARTSYS; /*
									Restart the system call transparently by the kernel itself
									if the process is interrupted by the signal
								*/
		}
		/* Trim the storage */
		scull_trim(dev);
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
	struct scull_dev *scull_dev = filp->private_data;
	int quantum = scull_dev->quantum;
	int qset = scull_dev->qset;
	int itemsize = quantum * qset;
	int retval = 0;
	int node = 0, itemloc = 0, s_pos, q_pos;
	struct scull_qset *dptr;

	if(down_interruptible(&scull_dev->sem)) {
		return -ERESTARTSYS;
	}

	if(*f_pos >= scull_dev->size) {
		goto out;
	}

	if(*f_pos + count > scull_dev->size) {
		count = scull_dev->size - *f_pos;
	}

	/* Get the node in the list */
	node = *f_pos / itemsize;

	/* Get the location in the item */
	itemloc = *f_pos % itemsize;

	/* Get the qset location */
	s_pos = itemloc / quantum;

	/* Get the location inside the qset */
	q_pos = itemloc % quantum;

	dptr = scull_follow(scull_dev, node);

	if(dptr == NULL || !(dptr->data) || !(dptr->data[s_pos])) {
		goto out; /* Will not retrieve data from holes */
	}

	if(count > quantum - q_pos) {
		count = quantum - q_pos;
	}

	if(copy_to_user(buff, &dptr->data[s_pos], count)) {
		retval = -EFAULT;
		goto out;
	}

	*f_pos += count;
	retval = count;

	out:
		up(&scull_dev->sem);
		return retval;
}

ssize_t scull_write(
			struct file *filp,
			const char __user *buff,
			size_t count,
			loff_t *f_pos
			)
{
	struct scull_dev *scull_dev = filp->private_data;
	int quantum = scull_dev->quantum;
	int qset = scull_dev->qset;
	struct scull_qset *dptr;

	int itemsize = quantum * qset; /* Size of each node */
	ssize_t retval = -ENOMEM;
	int item, s_pos, q_pos, nodeloc;

	if(down_interruptible(&(scull_dev->sem))) {
		return -ERESTARTSYS;
	}

	/* Get the node "item" in the list */
	item = (long) *f_pos / itemsize;

	/* Get the location in the item */
	nodeloc = (long) *f_pos % itemsize;

	/* Get the location in the quantum set*/
	q_pos = nodeloc % quantum;

	/* Get quantum set in the quantum node */
	s_pos = nodeloc / quantum;

	dptr = scull_follow(scull_dev, item);

	if(dptr == NULL) {
		goto out;
	}

	if(!dptr->data) {
		dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);

		if(!dptr->data) {
			goto out;
		}
		memset(dptr->data, 0, qset * sizeof(char *));
	}

	if(!dptr->data[s_pos]) {
		dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
		if(!dptr->data) {
			goto out;
		}
	}

	/* Write only upto the end of the quantum */
	if(count > quantum - q_pos) {
		count = quantum - q_pos;
	}

	if(copy_from_user(&dptr->data[s_pos] + q_pos, buff, count)) {
		retval = -EFAULT;
		goto out;
	}

	*f_pos += count;
	retval = count;

	if(scull_dev->size < *f_pos) {
		scull_dev->size = *f_pos;
	}

	out:
		up(&(scull_dev->sem));
		return retval;
}

loff_t scull_llseek(struct file *filp, loff_t off, int whence)
{
	struct scull_dev *scull_dev = filp->private_data;
	loff_t newpos;

	switch(whence) {
		case 0: /* SEEK_SET */
			newpos = off;
			break;
		case 1: /* SEEK_CUR */
			newpos = filp->f_pos + off;
			break;
		case 2: /* SEEK_END */
			newpos = scull_dev->size + off;
			break;
		default: /* Should not happen */
			return -EINVAL;
	}

	if(newpos < 0) {
		return -EINVAL;
	}

	filp->f_pos = newpos;
	return newpos;
}

long scull_ioctl(struct file *filp,
					unsigned int cmd, unsigned long arg)
{
	int err = 0, tmp;
	int retval = 0;

	if(_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) {
		return -ENOTTY;
	}

	if(_IOC_NR(cmd) > SCULL_IOC_MAXNR) {
		return -ENOTTY;
	}

	if(_IOC_DIR(cmd) & _IOC_READ) {
		err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	} else if(_IOC_DIR(cmd) & _IOC_WRITE) {
		err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	}

	if(err) {
		return -EFAULT;
	}

	switch(cmd) {
		case SCULL_IOCRESET:
			scull_quantum = SCULL_QUANTUM;
			scull_qset = SCULL_QSET;
			break;
		case SCULL_IOCSQUANTUM: /* Set: arg is pointer to the value */
			if(!capable(CAP_SYS_ADMIN)) {
				return -EPERM;
			}
			retval = get_user(scull_quantum, (int __user *) arg);
			break;
		case SCULL_IOCSQSET:
			if(!capable(CAP_SYS_ADMIN)) {
				return -EPERM;
			}
			retval = get_user(scull_qset, (int __user *) arg);
			break;
		case SCULL_IOCTQUANTUM: /* Tell: arg is the value */
			if(!capable(CAP_SYS_ADMIN)) {
				return -EPERM;
			}
			scull_quantum = arg;
			break;
		case SCULL_IOCTQSET:
			if(!capable(CAP_SYS_ADMIN)) {
				return -EPERM;
			}
			scull_qset = arg;
			break;
		case SCULL_IOCGQUANTUM: /* Get: arg is pointer to result */
			retval = put_user(scull_quantum, (int __user *)arg);
			break;
		case SCULL_IOCGQSET: /* Get: arg is pointer to result */
			retval = put_user(scull_qset, (int __user *)arg);
			break;
		case SCULL_IOCQQUANTUM: /*Query: Return as is. Value is positive */
			return scull_quantum;
		case SCULL_IOCQQSET: /* Query: Return as is. Value is positive */
			return scull_qset;
		case SCULL_IOCXQUANTUM: /* eXchange. Set the new value, return the old value */
			if(!capable(CAP_SYS_ADMIN)) {
				return -EPERM;
			}
			tmp = scull_quantum;
			retval = get_user(scull_quantum, (int __user *)arg);
			if(retval == 0) {
				retval = put_user(tmp, (int __user *)arg);
			}
			break;
		case SCULL_IOCXQSET:
			if(!capable(CAP_SYS_ADMIN)) {
				return -EPERM;
			}
			tmp = scull_qset;
			retval = get_user(scull_qset, (int __user *) arg);
			if(retval == 0) {
				retval = put_user(tmp, (int __user *)arg);
			}
			break;
		case SCULL_IOCHQUANTUM: /*sHift: Tell + Query, Query the old value. Set the value from arg */
			if(!capable(CAP_SYS_ADMIN)) {
				return -EPERM;
			}

			tmp = scull_quantum;
			scull_quantum = arg;
			return tmp;
		case SCULL_IOCHQSET:
			if(!capable(CAP_SYS_ADMIN)) {
				return -EPERM;
			}
			tmp = scull_qset;
			scull_qset = arg;
			return tmp;
		default:
			return -ENOTTY;
	}

	return retval;
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

	for(i = 0; i<scull_nr_devices; i++) {
		device_destroy(scull_class, MKDEV(scull_major, scull_minor + i));
	}

	class_unregister(scull_class);
	class_destroy(scull_class);

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

	/* Create a scull class */
	scull_class = class_create(THIS_MODULE, CLASS_NAME);

	if(IS_ERR(scull_class)) {
		/*TODO: Pending error handling */
		printk(KERN_DEBUG "Could not allocate class");
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
		scull_devices[i].quantum = scull_quantum;
		scull_devices[i].qset = scull_qset;
		sema_init(&(scull_devices[i].sem), 1);
		scull_setup_cdev(&(scull_devices[i]), i);
	}

	/* Register device driver */
	for(i = 0; i<scull_nr_devices; i++) {
		scull_devs[i] = device_create(scull_class, NULL, MKDEV(scull_major, scull_minor + i),
							 NULL, "%s%d", DEVICE_NAME, i);

		/* TODO: Robust error handling required */
		if(IS_ERR(scull_devs[i])) {
			printk(KERN_DEBUG "Could not create scull devices\n");
		}
	}


	return 0;

fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
