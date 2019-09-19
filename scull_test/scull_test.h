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
