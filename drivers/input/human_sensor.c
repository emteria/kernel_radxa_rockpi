#define MAJOR_NUM          60
#define MODULE_NAME                "MOTION"

#include "linux/kernel.h"
#include "linux/mm.h"
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <asm/memory.h>
#include <asm/unistd.h>
#include "linux/semaphore.h"
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>
#include <linux/fb.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include "asm-generic/int-ll64.h"
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/err.h>
#include <linux/version.h>      
#include <linux/fb.h>
//#define CONFIG_OF 1
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#define CFAKE_DEVICE_NAME "motion"

struct nix_sensor_info {
	struct platform_device	*pdev;
	int     sensor_det;
	struct cdev cdev;

};

struct nix_sensor_info *sensor_info;

static int debug = 1;
module_param(debug, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(debug, "Enable debug");

static int cfake_ndevices = 2;

static unsigned int cfake_major = 0;
static struct class *cfake_class = NULL;

#define printk(x...)			\
if ( debug ){				\
	printk(x);			\
}

/*
struct human_sensor_t{
    struct input_dev      *idev;
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend  early_suspend;
#endif
    volatile int suspend_indator;
};
static struct human_sensor_t *human_sensor = NULL;
*/
static int wake_on_motion= 0;

#define IOCTL_APP     71
#define SET_BIT   _IO(IOCTL_APP,1)
#define TURN_ON    _IO(IOCTL_APP,2)
#define TURN_OFF   _IO(IOCTL_APP,3)
#define POWER_STATE   _IO(IOCTL_APP,4)
#define USB_AS_HOST    _IO(IOCTL_APP,5)
#define USB_AS_DEVICE   _IO(IOCTL_APP,6)

static ssize_t drv_read(struct file *filp, char *buf, size_t count, loff_t *ppos)
{
    uint8_t io_state;    
     
    io_state = gpio_get_value(sensor_info->sensor_det);
    printk("human_sensor detected = [%d]\n", io_state);
    if (copy_to_user(buf,&io_state,1)) {
        printk("human_sensor could not write motion sensor value to userspace\n");
        return -EFAULT;
    }
    return 1;
}

static int drv_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static long drv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret= 0;
    int __user *p= (int __user *) arg;
    printk("######rocky###### 44444444444444- \n");
    switch (cmd) {
	case SET_BIT:	// legacy stuff from A20 platform it won't do anything on RK3188 platform
            printk("human_sensor ioctl: (%d -> %d) addr %p\n", wake_on_motion,*p,p);
            ret= get_user(wake_on_motion, p);
            if (wake_on_motion == 0) {
                printk("humman_sensor disable wake on motion\n");    
            } else {
                printk("humman_sensor enable wake on motion\n");    
            }
            break;
	case TURN_ON:
	    printk("human_sensor power on human sensor\n");
	    break;
	case TURN_OFF:
	    printk("human_sensor power off human sensor\n");
	    break;
	case POWER_STATE:
	    printk("human_sensor return power on/off state : %d\n",ret);
	    return ret;
	    break;
        default:
            printk("human_sensor ioctl: invalid cmd %d\n", cmd);
            ret= -EINVAL;
            break;
    }
	printk("######rocky###### 777777777777777777- \n");
    if (ret != 0) {
		printk("######rocky###### 8888888888888888- \n");
        printk("human_sensor ioctl: FAIL %d\n", ret);
    }
    return ret;
}

static int drv_release(struct inode *inode, struct file *filp)
{
    return 0;
}

struct file_operations drv_fops =
{
    read:           drv_read,
    open:           drv_open,
    unlocked_ioctl: drv_ioctl,
    release:        drv_release,
};
    
static void cfake_cleanup_module(int devices_to_destroy)
{
    //int i;
    
    /* Get rid of character devices (if any exist) */
    device_destroy(cfake_class, MKDEV(cfake_major, 0));
    cdev_del(&sensor_info->cdev);
    
    if (cfake_class)
        class_destroy(cfake_class);

    /* [NB] cfake_cleanup_module is never called if alloc_chrdev_region()
     * has failed. */
    unregister_chrdev_region(MKDEV(cfake_major, 0), cfake_ndevices);
    return;
}

static int human_sensor_probe(struct platform_device *pdev)
{
    int ret = -1;
    int gpio;
    struct device_node *sensor_node = pdev->dev.of_node;
    int err = 0;
    int devices_to_destroy = 0;
    dev_t dev = 0;
    dev_t devno;
    struct device *device = NULL;
    printk("######rocky###### 1111111111111111111111111- \n");
    sensor_info = kzalloc(sizeof(struct nix_sensor_info), GFP_KERNEL);
    if (!sensor_info)
    {
        err = -ENOMEM;
        goto failed_1;
    }

    // request GPIO resource
    sensor_info->pdev = pdev;
    gpio = of_get_named_gpio_flags(sensor_node,"motion_det", 0,NULL);
    if (!gpio_is_valid(gpio)){
    	printk("invalid motion_det: %d\n",gpio);
    	return -1;
    }
    sensor_info->sensor_det = gpio;
		gpio_free(sensor_info->sensor_det);
		
    if ( gpio_request(sensor_info->sensor_det, "motion_detected") < 0 )
    {
			printk("human sensor failure to request GPIO PIN1_PC4\n");
    	gpio_free(sensor_info->sensor_det);
    	ret = -EIO;
    	goto failed_1;	
    }

    if ( gpio_direction_input(sensor_info->sensor_det) < 0 )
    {
			printk("human sensor failure to configure GPIO PIN1_PC4\n");
    	ret = -EIO;
    	goto failed_1;	
    }
    printk("######rocky###### 2222222222222222222- \n");
    // create char driver 
    if (cfake_ndevices <= 0)
    {
        printk(KERN_WARNING "[target] Invalid value of cfake_ndevices: %d\n", 
            cfake_ndevices);
        err = -EINVAL;
        return err;
    }
    
    // Get a range of minor numbers (starting with 0) to work with 
    err = alloc_chrdev_region(&dev, 0, cfake_ndevices, MODULE_NAME);
    if (err < 0) {
        printk(KERN_WARNING "[target] alloc_chrdev_region() failed\n");
        return err;
    }
    cfake_major = MAJOR(dev);
  
    // Create device class (before allocation of the array of devices) 
    cfake_class = class_create(THIS_MODULE, CFAKE_DEVICE_NAME);
    if (IS_ERR(cfake_class)) {
          err = PTR_ERR(cfake_class);
          goto failed_1;
    }
    printk("######rocky###### 33333333333333- \n");
    devno= MKDEV(cfake_major, 0);
       
    cdev_init(&sensor_info->cdev, &drv_fops);
    sensor_info->cdev.owner = THIS_MODULE;
    
    err = cdev_add(&sensor_info->cdev, devno, 1);
    if (err)
    {
        printk(KERN_WARNING "[target] Error %d while trying to add %s%d",err, CFAKE_DEVICE_NAME, 0);
        return err;
    }
    
    device = device_create(cfake_class, NULL, 
        devno, NULL, 
        CFAKE_DEVICE_NAME "%d", 0);
    
    if (IS_ERR(device)) {
        err = PTR_ERR(device);
        printk(KERN_WARNING "[target] Error %d while trying to create %s%d",err, CFAKE_DEVICE_NAME, 0);
        cdev_del(&sensor_info->cdev);
        return err;
    }        
    printk("test human_sensor io=%d\n",gpio_get_value(sensor_info->sensor_det));
    return 0;  //return Ok

failed_1:
    cfake_cleanup_module(devices_to_destroy);
    return ret;
}

static int human_sensor_remove(struct platform_device *pdev)
{
    gpio_free(sensor_info->sensor_det);
    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id of_rk_nix_sensor_match[] = {
	{ .compatible = "nix-sensor" },
	{ /* Sentinel */ }
};
#endif

static struct platform_driver human_sensor_driver = {
	.probe		= human_sensor_probe,
	.remove		= human_sensor_remove,
	.driver		= {
	.name	= "nix-sensor",
	.owner	= THIS_MODULE,
#ifdef CONFIG_OF
	.of_match_table	= of_rk_nix_sensor_match,
#endif
	},

};

static int __init human_sensor_init(void)
{
    printk(KERN_INFO "Enter %s\n", __FUNCTION__);
    return platform_driver_register(&human_sensor_driver);
}

static void __exit human_sensor_exit(void)
{
    printk(KERN_INFO "Enter %s\n", __FUNCTION__);	
    platform_driver_unregister(&human_sensor_driver);
    cfake_cleanup_module(cfake_ndevices);
}

subsys_initcall(human_sensor_init);
module_exit(human_sensor_exit);

MODULE_DESCRIPTION("motion sensor driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:motion");

