#ifndef _SCULL_TEST_H_
#define _SCULL_TEST_H_

#ifndef SCULL_MAJOR
#define SCULL_MAJOR 0
#endif

#ifndef SCULL_NR_DEVICES
#define SCULL_NR_DEVICES 4
#endif

#ifndef SCULL_QUANTUM
#define SCULL_QUANTUM 	4000
#endif

#ifndef SCULL_QSET
#define SCULL_QSET 	1000
#endif

#define SCULL_IOC_MAGIC 	'k'

#define SCULL_IOCRESET 		_IO(SCULL_IOC_MAGIC, 0)

#define SCULL_IOCSQUANTUM 	_IOW(SCULL_IOC_MAGIC, 1, int)
#define SCULL_IOCSQSET 		_IOW(SCULL_IOC_MAGIC, 2, int)
#define SCULL_IOCTQUANTUM 	_IO(SCULL_IOC_MAGIC, 3)
#define SCULL_IOCTQSET 		_IO(SCULL_IOC_MAGIC, 4)
#define SCULL_IOCGQUANTUM 	_IOR(SCULL_IOC_MAGIC, 5, int)
#define SCULL_IOCGQSET 		_IOR(SCULL_IOC_MAGIC, 6, int)
#define SCULL_IOCQQUANTUM 	_IO(SCULL_IOC_MAGIC, 7)
#define SCULL_IOCQQSET 		_IO(SCULL_IOC_MAGIC, 8)
#define SCULL_IOCXQUANTUM 	_IOWR(SCULL_IOC_MAGIC, 9, int)
#define SCULL_IOCXQSET 		_IOWR(SCULL_IOC_MAGIC, 10, int)
#define SCULL_IOCHQUANTUM 	_IO(SCULL_IOC_MAGIC, 11)
#define SCULL_IOCHQSET 		_IO(SCULL_IOC_MAGIC, 12)

#define SCULL_P_IOCTSIZE 	_IO(SCULL_IOC_MAGIC, 13)
#define SCULL_P_IOCQSIZE 	_IO(SCULL_IOC_MAGIC, 14)

#define SCULL_IOC_MAXNR 	14

#define CLASS_NAME 		"scull"
#define DEVICE_NAME 	"scull"

extern int scull_major;
extern int scull_minor;
extern int scull_nr_devices;
extern int scull_quantum;
extern int scull_qset;

struct scull_qset {
	void **data;
	struct scull_qset *next;
};

struct scull_dev {
	struct scull_qset *data;
	int quantum;
	int qset;
	unsigned long size;
	struct semaphore sem;
	struct cdev cdev;
};

#endif /* _SCULL_TEST_H_ */
