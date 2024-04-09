#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <sound/core.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

//#define power_io
//#define LED_POWER_GPIO 131 //GPIO4_A3
int LED_POWER_GPIO;//rocky 
spinlock_t my_lock;
struct led_con_t{
	int zigbee_en_gpio;
	int led_gpio;
	int zigbee_en_flag;
	int zigbee_reset_flag;

	
	struct hrtimer timer;
	ktime_t light_poll_delay;
	unsigned int test_code;
	int header_code_flag;
	int data_1_flag ;
	int h_1_flag;
	int num;
	int code_end_flag;
};
static struct led_con_t *led_con = NULL;
//static int sleepstatus=0;
static struct delayed_work i_delayed_work;

//led_status   1:on   0:off
static int  led_status=0;

//extern int vcc_sd_enable(int value);
static const struct of_device_id of_rk_nix_sensor_match[] = {
	{ .compatible = "led_con_h" },
	{ /* Sentinel */ }
};
///////////////////hrtimer_start(&led_con_t->timer, led_con_t->light_poll_delay, HRTIMER_MODE_REL);/////////////////////////////////////////////



void set_gpio_h(void){
	//if(gpio_direction_output(led_con->led_gpio,1) < 0)
	//	printk("led_gpio h fail \n");
	gpio_set_value(led_con->led_gpio,1);//rocky.sun
//	printk("1");
	//return ;
}
void set_gpio_l(void){
	//if(gpio_direction_output(led_con->led_gpio,0) < 0)
	//	printk("led_gpio l fail \n");
	gpio_set_value(led_con->led_gpio,0);//rocky.sun
//	printk("0");
	//return ;
}

static enum hrtimer_restart jsa_als_timer_func(struct hrtimer *timer)
{
	unsigned int data;
	struct led_con_t *led_con = container_of(timer, struct led_con_t, timer);

	//printk("timer test_code ===  %x ============\n", led_con->test_code);
	//set_gpio_h();
	if(led_con->code_end_flag == 1){
        
		set_gpio_h();
		led_con->data_1_flag = 1;
		led_con->header_code_flag = 1;
		led_con->num = 31;
		led_con->code_end_flag = 0;
		return 0;
	}
	if(led_con->header_code_flag == 1){
		set_gpio_h();
		led_con->data_1_flag = 1;
		led_con->header_code_flag = 0;
		led_con->num = 31;
		led_con->code_end_flag = 0;
		led_con->light_poll_delay = ns_to_ktime(4450*1000);
		hrtimer_start(&led_con->timer, led_con->light_poll_delay, HRTIMER_MODE_REL);
		return 0;
	}
    if(led_con->num < 0){
		set_gpio_l();
		led_con->code_end_flag = 1;
		//led_con->light_poll_delay = ns_to_ktime(15*1000*1000);
		led_con->light_poll_delay = ns_to_ktime(550*1000);
		hrtimer_start(&led_con->timer, led_con->light_poll_delay, HRTIMER_MODE_REL);
    	//hrtimer_cancel(&led_con->timer);
		return 0;
    }
	data = led_con->test_code;
	if(led_con->data_1_flag == 1){
			set_gpio_l();
			led_con->data_1_flag = 0;
			led_con->light_poll_delay = ns_to_ktime(550*1000);
			hrtimer_start(&led_con->timer, led_con->light_poll_delay, HRTIMER_MODE_REL);
			return 0;
	}else{
			set_gpio_h();
			led_con->data_1_flag = 1;
			led_con->h_1_flag = (data >> (led_con->num)) & 0x01;//(data  &(0x01 << (32 - led_con->num)));
			led_con->num = led_con->num - 1;
//			printk("led_con->num = %d led_con->h_1_flag = %d ,\n",led_con->num,led_con->h_1_flag);
			if(led_con->h_1_flag == 1){
				led_con->light_poll_delay = ns_to_ktime(1580*1000);
			}	
			else{
				led_con->light_poll_delay = ns_to_ktime(490*1000);
			}
			
			hrtimer_start(&led_con->timer, led_con->light_poll_delay, HRTIMER_MODE_REL);
			return 0;
	}
    
	return 0;
}


void end_data(void){
	set_gpio_h();
	hrtimer_cancel(&led_con->timer);
}
void led_code_write(unsigned int test_code){
spin_lock(&my_lock);
	led_con->header_code_flag = 1;
	set_gpio_l();
	led_con->test_code = test_code; //green 0x00ffa05f
	led_con->light_poll_delay = ns_to_ktime(9100*1000);
	hrtimer_start(&led_con->timer, led_con->light_poll_delay, HRTIMER_MODE_REL);
spin_unlock(&my_lock);
}
EXPORT_SYMBOL(led_code_write);
////////////////////////////////////////////////////////////////



void led_power_io(int status)
{
	#ifdef power_io
		/*if ( gpio_direction_output(led_con->zigbee_en_gpio,1) < 0 ){
			printk("zigbee_en_gpio output fail\n");
    		return ;	
		}*/
			gpio_set_value(led_con->zigbee_en_gpio,1);//rocky.sun
			//return ;
	#else
		
		//vcc_sd_enable(status);
	#endif
}
void led_reset_io(int status)
{

		/*if ( gpio_direction_output(led_con->led_gpio,status) < 0 ){
			printk("led_gpio output fail\n");
    		return ;	
		}*/
			gpio_set_value(led_con->led_gpio,status);//rocky.sun
			//return ;

}	

int get_code(int code){
	int temp,i;
	int high4,low4;
	low4 = (code&0x0f)<<4;
	high4= (code>>4);
	
	for(i=0;i<4;i++){
		if((low4>>i)|0x01){
			low4=(low4|0x01)<<(4-i-1);
		}
		else{
		low4=(low4|0x00)<<(4-i-1);
		}
	}
	
		for(i=0;i<4;i++){
		if((high4>>i)|0x01){
			high4=(high4|0x01)<<(4-i-1);
		}
		else{
		high4=(high4|0x00)<<(4-i-1);
		}
	}
	printk("xiao--low4=%x\n",low4);
	printk("xiao--high4=%x\n",high4);
	temp=(low4<<4)|(high4&&0x0f);
	return temp;
}
static ssize_t zigbee_reset_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	char cmd;
	unsigned int test_code;
	sscanf(buf, "%c %x", &cmd,&test_code);
	if (cmd == 'r'){
		led_power_io(0);
		led_reset_io(0);
		msleep(350);
		led_power_io(1);
		msleep(50);
		led_reset_io(1);
		
	} else if(cmd == 'h'){
		led_power_io(1);
		led_reset_io(1);
	}else if(cmd == 'l'){
		led_power_io(0);
		led_reset_io(0);
	}else if(cmd == 'w'){		
		if(test_code==0x02)
		{
			printk("test_code = %x  \n",test_code);
			test_code=0x00f740bf;	
			led_status=0;
			led_code_write(test_code);				
		}
		if(test_code==0x03)
		{
			printk("test_code = %x  \n",test_code);
			test_code=0x00f7c03f;	
			led_status=1;			
		}
		if(!led_status)
		{
			return count;
		}
		printk("test_code = %x  \n",test_code);
		if(test_code==0x04){
		test_code=0x00f720df;
		}
		if(test_code==0x05){
		test_code=0x00f7a05f;
		}
		if(test_code==0x06){
		test_code=0x00f7609f;
		}
		if(test_code==0x08){
		test_code=0x00f710ef;
		}
		if(test_code==0x09){
		test_code=0x00f7906f;
		}
		if(test_code==0x0a){
		test_code=0x00f750af;
		}
		if(test_code==0x0c){
		test_code=0x00f730cf;
		}
		if(test_code==0x0d){
		test_code=0x00f7b04f;
		}
		if(test_code==0x0e){
		test_code=0x00f7708f;
		}
		if(test_code==0x10){
		test_code=0x00f708f7;
		}
		if(test_code==0x11){
		test_code=0x00f78877;
		}
		if(test_code==0x12){
		test_code=0x00f748b7;
		}
		if(test_code==0x14){
		test_code=0x00f728d7;
		}
		if(test_code==0x15){
		test_code=0x00f7a857;
		}
		if(test_code==0x16){
		test_code=0x00f76897;
		}
		if(test_code==0x00){
		test_code=0x00f700ff;
		}
		if(test_code==0x01){
		test_code=0x00f7807f;
		}	
		if(test_code==0x07){
		test_code=0x00f7e01f;
		}
		if(test_code==0x0b){
		test_code=0x00f7d02f;
		}
		if(test_code==0x0f){
		test_code=0x00f7f00f;
		}
		if(test_code==0x13){
		test_code=0x00f7c837;
		}
		if(test_code==0x17){
		test_code=0x00f7e817;
		}
		led_code_write(test_code);
	}
	else
		printk("command error \n");
	//led_code_write();
	return count;
}

static ssize_t zigbee_reset_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;//sprintf(buf, "power %d , reset %d.\n",gpio_get_value(led_con->zigbee_en_gpio) ,gpio_get_value(led_con->led_gpio));
}

static struct device_attribute zigbee_reset_attr[] = {
	__ATTR(zigbee_reset, 0664, zigbee_reset_read,zigbee_reset_store),
};

static void i_once_work(struct work_struct *work)
{
 //led_code_write(0x00f7c03f);
 msleep(150);
 led_code_write(0x00f7aa55);
 msleep(500);
 printk("####rocky### ledcon-i_once_work aa55!\r\n");
 gpio_direction_output(led_con->led_gpio,1);

 msleep(500);
 led_code_write(0x00f7aa55);
 msleep(100);
 led_status=1;
}

static int led_con_probe(struct platform_device *pdev)
{
	enum of_gpio_flags flags;
    struct device_node *sensor_node = pdev->dev.of_node;
	int ret;
 
    printk("led_con_probe start\n\n\n\n");
    led_con = kzalloc(sizeof(struct led_con_t), GFP_KERNEL);
    if (!led_con)
    {
		printk("zigbee kzalloc fail");
        goto failed;
    }
	
	//add by rocky for led power
	ret = of_property_read_u32(sensor_node, "led-power", &LED_POWER_GPIO);
    if (ret < 0) {
        dev_err(&pdev->dev, "led_data: left-power missing\n");
        return ret;
    }
	
#ifdef power_io
    led_con->zigbee_en_gpio = of_get_named_gpio_flags(sensor_node,"zigbee-en-gpio", 0,&flags);
    if (!gpio_is_valid(led_con->zigbee_en_gpio)){
    	printk("zigbee-en-gpio: %d  fail \n",led_con->zigbee_en_gpio);
    	return -1;
    }
	led_con->zigbee_en_flag = (flags & OF_GPIO_ACTIVE_LOW)? 1:0;
#endif
	
	led_con->led_gpio = of_get_named_gpio_flags(sensor_node,"led-gpio", 0,&flags);
    if (!gpio_is_valid(led_con->led_gpio)){
    	printk("led_gpio: %d  fail \n",led_con->led_gpio);
    	return -1;
    }
	gpio_direction_output(led_con->led_gpio,1);
	led_con->zigbee_reset_flag = (flags & OF_GPIO_ACTIVE_LOW)? 0:1;
#ifdef power_io	
    if ( gpio_request(led_con->zigbee_en_gpio, "zigbee_en_gpio") < 0 ){

		printk("zigbee_en_gpio request  fail\n");
    	gpio_free(led_con->zigbee_en_gpio);
    	goto failed;	
    }
	//add by rocky.sun
	if ( gpio_direction_output(led_con->zigbee_en_gpio,1) < 0 ){
			printk("zigbee_en_gpio output fail\n");
    		//return ;	
		}
#endif
    if ( gpio_request(led_con->led_gpio, "led_gpio") < 0 ){

		printk("led_gpio request fail\n");
    	gpio_free(led_con->led_gpio);
    	goto failed;	
    }
	//add by rocky.sun
	if ( gpio_direction_output(led_con->led_gpio,0) < 0 ){
			printk("led_gpio output fail\n");
    		//return ;	
		}

	if (sysfs_create_file(&pdev->dev.kobj,&zigbee_reset_attr[0].attr))
			printk("zigbee sysfs_create_file fail\n");
	
	
	hrtimer_init(&led_con->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	led_con->light_poll_delay = ns_to_ktime(20 * NSEC_PER_MSEC);
	led_con->timer.function = jsa_als_timer_func;

	
	INIT_DELAYED_WORK(&i_delayed_work, i_once_work); 	
	schedule_delayed_work(&i_delayed_work,msecs_to_jiffies(3000));
	spin_lock_init(&my_lock);
	//msleep(50);
	//led_code_write(0x00f7609f);
	//msleep(50);
	//led_code_write(0x00f7609f);
	return 0;
failed:
    return -1;
}

static int led_con_remove(struct platform_device *pdev)
{	
	if(led_con->led_gpio != 0)
		gpio_free(led_con->led_gpio);
#ifdef power_io	
	if(led_con->zigbee_en_gpio != 0)
		gpio_free(led_con->zigbee_en_gpio);
#endif
    return 0;
}
static struct platform_driver led_con_driver = {
	.probe		= led_con_probe,
	.remove		= led_con_remove,
	.driver		= {
	.name	= "led_con_h",
	.owner	= THIS_MODULE,
	.of_match_table	= of_match_ptr(of_rk_nix_sensor_match),
	},

};

void ledcon_sleepc(int status)
{
	printk("rocky----ledcon_sleepc-off----\n");
    
	gpio_set_value(LED_POWER_GPIO,0);
}
EXPORT_SYMBOL(ledcon_sleepc);
void ledcon_resumec(int status)
{
	printk("rocky--led---pwm_backlight_power_on---status=1  or status=0  ----\n");
    gpio_set_value(LED_POWER_GPIO,1);
    msleep(1000);
    led_code_write(0x00f7aa55);
	msleep(100);
	led_code_write(0x00f7aa55);
	printk("rocky--status=1---ledcon_resumec----on----\n");
}
EXPORT_SYMBOL(ledcon_resumec);
static int __init led_con_init(void)
{
    printk(KERN_INFO "Enter hyman 11 %s\n", __FUNCTION__);
    return platform_driver_register(&led_con_driver);
}

static void __exit led_con_exit(void)
{
    printk(KERN_INFO "Enter %s\n", __FUNCTION__);	
    platform_driver_unregister(&led_con_driver);
}

subsys_initcall(led_con_init);
module_exit(led_con_exit);

MODULE_DESCRIPTION("zigbee sensor driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:zigbee");
