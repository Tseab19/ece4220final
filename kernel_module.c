#ifndef MODULE
#define MODULE
#endif
#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#define MSG_SIZE 50
#define CDEV_NAME "Project"	// "YourDevName"
static int major;
static char msg[MSG_SIZE];

int freq = 0;
//pointers used to point to each part of the register
unsigned long * BASE; //GPFSEL
unsigned long * GPFSEL0;
unsigned long * GPFSEL1;
unsigned long * GPFSEL2;
unsigned long * GPSET;
unsigned long * GPSCLR;
unsigned long * PUD; //Pull up/down register
unsigned long * PUD_CLK;
unsigned long * EDGE; //rising edge detection
unsigned long * EVENT; //event detetction register
MODULE_LICENSE("GPL");

int mydev_id; //character device

static ssize_t device_write(struct file *filp, const char __user *buff, size_t len, loff_t *off){
  //not needed
}

static ssize_t device_read(struct file *filp, const char __user *buff, size_t len, loff_t *off){
  //need to implement sending the button press info from msg here
  ssize_t dummy = copy_to_user(buff, msg, len);
	if(dummy == 0 && msg[0] != '\0'){
		printk("%s\n", msg);
  }
	msg[0] = '\0';
	return 1;
}

// structure needed when registering the Character Device. Members are the callback
// functions when the device is read from or written to.
static struct file_operations fops = {
	.read = device_read,
	.write = device_write,
};

// structure for the kthread.
static struct task_struct *kthread1;
static irqreturn_t button_isr(int irq, void *dev_id){
	disable_irq_nosync(79); //disable interrupt

	unsigned long result = *EVENT & 0x1F0000;

	switch(result){
	case 0x100000:
		printk("Button 5 pushed\n"); //used for testing to see in dmesg
		msg[0] = '5'; //message to be sent when device read is called
		break;
	case 0x80000:
		printk("Button 4 pushed\n");
    msg[0] = '4'; //message to be sent when device read is called
		break;
	case 0x40000:
		printk("Button 3 pushed\n");
    msg[0] = '3'; //message to be sent when device read is called
		break;
	case 0x20000:
		printk("Button 2 pushed\n");
    msg[0] = '2'; //message to be sent when device read is called
		break;
	case 0x10000:
		printk("Button 1 pushed\n");
    msg[0] = '1'; //message to be sent when device read is called
		break;

    //add switch cases here
	default:
		printk("Case defaulted\n");
		break;
	}

	*EVENT |= 0x1F0000; //clear event register
	enable_irq(79);	//enable interrupt

    return IRQ_HANDLED;
}

int thread_init(void)
{
	int requestReturn;

  //register charcter device
	major = register_chrdev(0, CDEV_NAME, &fops);
	if (major < 0) {
     		printk("Registering the character device failed with %d\n", major);
	     	return major;
	}
  //used for setting up the character device
	printk("Lab6_cdev_kmod example, assigned major: %d\n", major);
	printk("Create Char Device (node) with: sudo mknod /dev/%s c %d 0\n", CDEV_NAME, major);

  //bit mapping to set up registers
	BASE = ioremap(0x3F200000, 4096); //map to the base
	PUD = BASE + 0x94/4; //pull up/down resistor
	PUD_CLK = BASE + 0x98/4;

	EDGE = BASE + 0x4C/4;
	EVENT = BASE + 0x40/4;

	GPFSEL0 = BASE;
	GPFSEL1 = BASE + 0x4/4;
	GPFSEL2 = BASE + 0x8/4;
	GPSET = BASE + 0x1C/4;
	GPSCLR = BASE + 0x28/4;

	*GPFSEL1 &= 0xC003FFFF; //first 4 buttons as inputs
	*GPFSEL2 &= 0xFFFFFFF8; //last button as input

  //set switches as inputs

	//PUD for buttons
	*PUD |= 0x01;
	udelay(100);
	*PUD_CLK |= 0x1F0000;
	udelay(100);
	*PUD &= 0xFFFFFEAA; //turn off PUD signal
	*PUD_CLK &= 0xFFE0FFFF; //turn off PUD CLK signal
	udelay(100);

	//Rising edge detection
	*EDGE |= 0x1F0000;

  //need to set up switch detection here

	//Request interrupt
	requestReturn = request_irq(79, button_isr, IRQF_SHARED, "Button_handler", &mydev_id);
	printk("Button Detection enabled.\n");
    return 0;
}

void thread_cleanup(void) {
	*EDGE = *EDGE & 0xffe0ffff; //clear registers
	*EVENT = *EVENT | 0x001f0000;
	free_irq(79, &mydev_id); //release the interupt handler

	unregister_chrdev(major, CDEV_NAME);
	printk("Char Device /dev/%s unregistered.\n", CDEV_NAME);
}

// Notice this alternative way to define your init_module()
// and cleanup_module(). "thread_init" will execute when you install your
// module. "thread_cleanup" will execute when you remove your module.
// You can give different names to those functions.
module_init(thread_init);
module_exit(thread_cleanup);
