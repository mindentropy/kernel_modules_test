#ifndef _SCULL_TEST_H_
#define _SCULL_TEST_H_

#ifndef SCULL_MAJOR
#define SCULL_MAJOR 0
#endif

#ifndef SCULL_NR_DEVICES
#define SCULL_NR_DEVICES 4
#endif

extern int scull_major;
extern int scull_minor;
extern int scull_nr_devices;
extern int scull_quantum;
extern int scull_qset;

struct scull_dev {
	struct semaphore sem;
	struct cdev cdev;
};

#endif /* _SCULL_TEST_H_ */
