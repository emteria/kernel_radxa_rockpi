#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/fb.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/leds_pwm.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
struct pwm_para {
    int pwm_id;
    unsigned int period;
    unsigned int level;
    unsigned int duty;
    struct pwm_device *pwm;
};

struct wa_led{
	struct pwm_para left;
	struct pwm_para right;
};

static int left_ledstatus=0;
static int left_pwmstatus=0;
static int right_ledstatus=0;
static int right_pwmstatus=0;
static int sleepstatus=0;


struct wa_led led_data;
#define PWM_SEEK_RED_RIGHT  0xa1
#define PWM_SEEK_GREEN_RIGHT  0xa2
#define PWM_SEEK_BLUE_RIGHT  0xa3
#define PWM_SEEK_GREEN_BLUE_RIGHT 0xa4
#define PWM_SEEK_RED_BLUE_RIGHT 0xa5
#define PWM_SEEK_GREEN_RED_RIGHT 0xa6
#define PWM_SEEK_ALL_RIGHT 0Xa7

#define PWM_SEEK_RED_LEFT  0xb1
#define PWM_SEEK_GREEN_LEFT  0xb2
#define PWM_SEEK_BLUE_LEFT  0xb3
#define PWM_SEEK_GREEN_BLUE_LEFT 0xb4
#define PWM_SEEK_RED_BLUE_LEFT 0xb5
#define PWM_SEEK_GREEN_RED_LEFT 0xb6
#define PWM_SEEK_ALL_LEFT 0Xb7

#define LED_OFF 0x99

int LED_LEFT_RED,LED_LEFT_GREEN,LED_LEFT_BLUE,LED_RIGHT_RED,LED_RIGHT_GREEN,LED_RIGHT_BLUE;//rocky

/*
//R92
#define LED_LEFT_GREEN  127//GPIO3_D7
#define LED_LEFT_BLUE  125//GPIO3_D5
#define LED_LEFT_RED   126//GPIO3_D6

#define LED_RIGHT_GREEN  124//GPIO3_D4 
#define LED_RIGHT_BLUE   122//GPIO3_D2
#define LED_RIGHT_RED    123//GPIO3_D3*/

/*
//R103
#define LED_LEFT_GREEN  93//GPIO2_D5
#define LED_LEFT_BLUE  91//GPIO2_D3
#define LED_LEFT_RED   92//GPIO2_D4
#define LED_RIGHT_GREEN  90//GPIO2_D2 
#define LED_RIGHT_BLUE   88//GPIO2_D0
#define LED_RIGHT_RED    89//GPIO2_D1*/

#define led_MAJOR           96	/* Local major number. */
static struct class *yf_class;
#define led_NAME    "elc_led"
#if 0
static void led_pwm_cleanup(struct led_pwm_priv *priv)
{
	while (priv->num_leds--) {
		;//led_classdev_unregister(&priv->leds[priv->num_leds].cdev);
	}
}

static inline size_t sizeof_pwm_leds_priv(int num_leds)
{
	return sizeof(struct led_pwm_priv) +
			  (sizeof(struct led_pwm_data) * num_leds);
}
	

static int led_pwm_add(struct device *dev, struct led_pwm_priv *priv,
			   struct led_pwm *led, struct device_node *child)
{
	struct led_pwm_data *led_data = &priv->leds[priv->num_leds];
	int ret;
	
	led_data->cdev.name = led->name;
/*	led_data->active_low = led->active_low;
	led_data->cdev.name = led->name;
	led_data->cdev.default_trigger = led->default_trigger;
	led_data->cdev.brightness_set = led_pwm_set;
	led_data->cdev.brightness = LED_OFF;
	led_data->cdev.max_brightness = led->max_brightness;
	led_data->cdev.flags = LED_CORE_SUSPENDRESUME;*/
	printk("led_pwm = %s\n",led->name);
	if (child)
		led_data->pwm = devm_of_pwm_get(dev, child, NULL);
	else
		led_data->pwm = devm_pwm_get(dev, led->name);
	if (IS_ERR(led_data->pwm)) {
		ret = PTR_ERR(led_data->pwm);
		dev_err(dev, "unable to 1111 request PWM for %s: %d\n",
			led->name, ret);
		return ret;
	}
/*
	led_data->can_sleep = pwm_can_sleep(led_data->pwm);
	if (led_data->can_sleep)
		INIT_WORK(&led_data->work, led_pwm_work);

	led_data->period = pwm_get_period(led_data->pwm);
	if (!led_data->period && (led->pwm_period_ns > 0))
		led_data->period = led->pwm_period_ns;
*/

	return ret;
}


static int led_pwm_create_of(struct device *dev, struct led_pwm_priv *priv)
{
	struct device_node *child;
	struct led_pwm led;
	int ret = 0;

	memset(&led, 0, sizeof(led));

	for_each_child_of_node(dev->of_node, child) {
		led.name = of_get_property(child, "label", NULL) ? :
			   child->name;

		of_property_read_u32(child, "max-brightness",
					 &led.max_brightness);

		ret = led_pwm_add(dev, priv, &led, child);
		if (ret) {
			of_node_put(child);
			break;
		}
	}

	return ret;
}
#endif


static void pwm_wa_update(int left, int right)
{
	if(left == 1){
    	led_data.left.duty = led_data.left.level * led_data.left.period / 15;
    	pwm_config(led_data.left.pwm, led_data.left.duty, led_data.left.period);
	}
	if(right == 1){
    	led_data.right.duty = led_data.right.level * led_data.right.period / 15;
    	pwm_config(led_data.right.pwm, led_data.right.duty, led_data.right.period);
	}
}

static void pwm_wa_enable(int left, int right)
{
	if(left == 1){
    	pwm_enable(led_data.left.pwm);
	}
	if(right == 1){
    	pwm_enable(led_data.right.pwm);
	}	
}


static void pwm_wa_disable(int left, int right)
{
	if(left == 1){
    	// 关闭pwm之前将占空比设置为0
    	led_data.left.level = 0;
    	pwm_wa_update(1,0);
    	pwm_disable(led_data.left.pwm);
    }
 	if(right == 1){
    	// 关闭pwm之前将占空比设置为0
    	led_data.right.level = 0;
    	pwm_wa_update(0,1);
    	pwm_disable(led_data.right.pwm);
    }
 
}

/*
static void pwm_wa_set_period(unsigned int period)
{
    led_data.left.period = period;
    led_data.right.period = period;

    pwm_set_period(led_data.left.pwm, led_data.left.period);
    pwm_set_period(led_data.left.pwm, led_data.right.period);
}

static unsigned int pwm_wa_get_period(void)
{
    return 0 ;
}
*/
static ssize_t pwm_wa_parse_dt(struct platform_device *pdev)
{
    struct device_node *node = pdev->dev.of_node;
    int ret;

    if (!node) {
        dev_err(&pdev->dev, "led_data: Device Tree node missing\n");
        return -EINVAL;
    }
    
    ret = of_property_read_u32(node, "left-pwm-id", &led_data.left.pwm_id);
    if (ret < 0) {
        dev_err(&pdev->dev, "led_data: pwm-id missing\n");
        return ret;
    }
	
	printk("pwm wa parse dt left-pwm-id is %d !\n",led_data.left.pwm_id);

    ret = of_property_read_u32(node, "left-period", &led_data.left.period);
    if (ret < 0) {
        dev_err(&pdev->dev, "led_data: period missing\n");
        return ret;
    }

    ret = of_property_read_u32(node, "left-default-level", &led_data.left.level);
    if (ret < 0) {
        dev_err(&pdev->dev, "led_data: default-level missing\n");
        return ret;
    }

    ret = of_property_read_u32(node, "right-pwm-id", &led_data.right.pwm_id);
    if (ret < 0) {
        dev_err(&pdev->dev, "led_data: pwm-id missing\n");
        return ret;
    }
	
    printk("pwm wa parse dt right-pwm-id is %d ! \n",led_data.right.pwm_id);
	
    ret = of_property_read_u32(node, "right-period", &led_data.right.period);
    if (ret < 0) {
        dev_err(&pdev->dev, "led_data: period missing\n");
        return ret;
    }
    
    ret = of_property_read_u32(node, "right-default-level", &led_data.right.level);
    if (ret < 0) {
        dev_err(&pdev->dev, "led_data: default-level missing\n");
        return ret;
    }

	//add by rocky for gpio------------------------------
	ret = of_property_read_u32(node, "left-red", &LED_LEFT_RED);
    if (ret < 0) {
        dev_err(&pdev->dev, "led_data: left-red missing\n");
        return ret;
    }

	ret = of_property_read_u32(node, "left-green", &LED_LEFT_GREEN);
    if (ret < 0) {
        dev_err(&pdev->dev, "led_data: left-green missing\n");
        return ret;
    }

	ret = of_property_read_u32(node, "left-blue", &LED_LEFT_BLUE);
    if (ret < 0) {
        dev_err(&pdev->dev, "led_data: left-blue missing\n");
        return ret;
    }

	ret = of_property_read_u32(node, "right-red", &LED_RIGHT_RED);
    if (ret < 0) {
        dev_err(&pdev->dev, "led_data: right-red missing\n");
        return ret;
    }

	ret = of_property_read_u32(node, "right-green", &LED_RIGHT_GREEN);
    if (ret < 0) {
        dev_err(&pdev->dev, "led_data: right-green missing\n");
        return ret;
    }

	ret = of_property_read_u32(node, "right-blue", &LED_RIGHT_BLUE);
    if (ret < 0) {
        dev_err(&pdev->dev, "led_data: right-blue missing\n");
        return ret;
    }
	//add by rocky for gpio------------------------------
    return 0;
}
#if 1
void update(int left,int right,int level){
	if(left == 1){
		led_data.left.level = level;
    	pwm_wa_update(1,0);
    	pwm_wa_enable(1,0);
	}
	if(right == 1){
    	led_data.right.level = level;
    	pwm_wa_update(0,1);
    	pwm_wa_enable(0,1);
	}
}
//extern int backlightStatus;
//int pwm_duty[]={0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
//int pwm_duty[]={0x0f,0x0e,0x0d,0x0c,0x0b,0x0a,0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00};
//int pwm_duty_single[]={0x00,0x00,0x01,0x01,0x02,0x02,0x03,0x03,0x04,0x04,0x05,0x05,0x06,0x06,0x07,0x07};

int pwm_duty[]={0x06,0x06,0x07,0x07,0x08,0x08,0x09,0x09,0x0a,0x0a,0x0b,0x0b,0x0c,0x0d,0x0e,0x0f};
int pwm_duty_single[]={0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
int pwm_duty_h[]={0x04,0x04,0x05,0x05,0x06,0x07,0x08,0x09,0x0a,0x0a,0x0b,0x0b,0x0c,0x0d,0x0e,0x0f};
int pwm_duty_white[]={0x09,0x09,0x09,0x0a,0x0a,0x0a,0x0b,0x0b,0x0b,0x0b,0x0c,0x0c,0x0d,0x0d,0x0e,0x0f};
int pwm_duty_purple[]={0x05,0x05,0x05,0x06,0x06,0x06,0x07,0x07,0x08,0x08,0x09,0x09,0x0a,0x0a,0x0b,0x0b};
int pwm_duty_red[]={0x04,0x04,0x05,0x05,0x06,0x06,0x07,0x07,0x08,0x08,0x09,0x09,0x09,0x0a,0x0a,0x0a};
//int pwm_duty_red[]={0x04,0x04,0x05,0x05,0x06,0x06,0x07,0x07,0x08,0x08,0x09,0x09,0x0a,0x0a,0x0b,0x0b};
static long led_ioctl(struct file *file,
                        unsigned int cmd,
                        unsigned long arg)
{
	//printk("qwang backlightStatus=%d\n",backlightStatus);
   // if(backlightStatus==1){
	//    return 0;
// }
   if(sleepstatus==1)
   {
	   return 0;
   }
   //printk("######rocky##### cmd=%d\n",cmd);
   switch(cmd) {
        case PWM_SEEK_RED_LEFT:		    
		    gpio_set_value(LED_LEFT_RED,1);
			gpio_set_value(LED_LEFT_BLUE,0);
			gpio_set_value(LED_LEFT_GREEN,0);
            update(1, 0,pwm_duty_red[arg]);	
			left_pwmstatus = pwm_duty_red[arg];
            left_ledstatus = 1;			
            break;
        case PWM_SEEK_BLUE_LEFT:		    
			gpio_set_value(LED_LEFT_RED,0);
			gpio_set_value(LED_LEFT_BLUE,1);
			gpio_set_value(LED_LEFT_GREEN,0);
			update(1, 0,pwm_duty_h[arg]);
            left_pwmstatus = pwm_duty_h[arg];			
            left_ledstatus	= 2 ;	
            break;
		case PWM_SEEK_GREEN_LEFT:		
			gpio_set_value(LED_LEFT_RED,0);
			gpio_set_value(LED_LEFT_BLUE,0);
			gpio_set_value(LED_LEFT_GREEN,1);
			update(1, 0,pwm_duty_h[arg]);
			left_pwmstatus = pwm_duty_h[arg];
			left_ledstatus	= 3 ;	
            break;	
		case PWM_SEEK_RED_RIGHT:		    
			gpio_set_value(LED_RIGHT_BLUE,0);
			gpio_set_value(LED_RIGHT_GREEN,0);
			gpio_set_value(LED_RIGHT_RED,1);
			update(0, 1,pwm_duty_red[arg]);
			right_pwmstatus = pwm_duty_red[arg];
            right_ledstatus = 1; 			
		    break;
	    case PWM_SEEK_BLUE_RIGHT:		   
			gpio_set_value(LED_RIGHT_BLUE,1);
			gpio_set_value(LED_RIGHT_GREEN,0);
			gpio_set_value(LED_RIGHT_RED,0);
			update(0, 1,pwm_duty_h[arg]);
			right_pwmstatus = pwm_duty_h[arg];
            right_ledstatus = 2; 			
		    break;
		case PWM_SEEK_GREEN_RIGHT:		  
			gpio_set_value(LED_RIGHT_BLUE,0);
			gpio_set_value(LED_RIGHT_GREEN,1);
			gpio_set_value(LED_RIGHT_RED,0);
			update(0, 1,pwm_duty_h[arg]);	
			right_pwmstatus = pwm_duty_h[arg];
	        right_ledstatus = 3; 			
		    break;
		case PWM_SEEK_GREEN_BLUE_RIGHT:		    
			gpio_set_value(LED_RIGHT_BLUE,1);
			gpio_set_value(LED_RIGHT_GREEN,1);
			gpio_set_value(LED_RIGHT_RED,0);
			update(0, 1,pwm_duty_h[arg]);	
			right_pwmstatus = pwm_duty_h[arg];
            right_ledstatus = 4; 	
		    break;
		case PWM_SEEK_GREEN_RED_RIGHT:		    
			gpio_set_value(LED_RIGHT_BLUE,0);
			gpio_set_value(LED_RIGHT_GREEN,1);
			gpio_set_value(LED_RIGHT_RED,1);
			update(0, 1,pwm_duty[arg]);	
			right_pwmstatus = pwm_duty[arg];
            right_ledstatus = 5; 				
		    break;	
       	case PWM_SEEK_RED_BLUE_RIGHT:		   
			gpio_set_value(LED_RIGHT_BLUE,1);
			gpio_set_value(LED_RIGHT_GREEN,0);
			gpio_set_value(LED_RIGHT_RED,1);
			update(0, 1,pwm_duty_purple[arg]);	
			right_pwmstatus = pwm_duty_purple[arg];
            right_ledstatus = 6; 		
		    break;
		case PWM_SEEK_ALL_RIGHT:		  
			gpio_set_value(LED_RIGHT_BLUE,1);
			gpio_set_value(LED_RIGHT_GREEN,1);
	  		gpio_set_value(LED_RIGHT_RED,1);
	  		update(0, 1,pwm_duty_white[arg]);	
			right_pwmstatus = pwm_duty_white[arg];
            right_ledstatus = 7; 				
		    break;
	   case PWM_SEEK_GREEN_BLUE_LEFT:	       
			gpio_set_value(LED_LEFT_BLUE,1);
			gpio_set_value(LED_LEFT_GREEN,1);
			gpio_set_value(LED_LEFT_RED,0);
			update(1, 0,pwm_duty_h[arg]);
			left_pwmstatus = pwm_duty_h[arg];			
            left_ledstatus	= 4 ;				
		    break;
		case PWM_SEEK_GREEN_RED_LEFT:		       
			gpio_set_value(LED_LEFT_BLUE,0);
			gpio_set_value(LED_LEFT_GREEN,1);
			gpio_set_value(LED_LEFT_RED,1);
			update(1, 0,pwm_duty[arg]);	
			left_pwmstatus = pwm_duty[arg];
            left_ledstatus	= 5 ;				
			break;	
		case PWM_SEEK_RED_BLUE_LEFT:				
			gpio_set_value(LED_LEFT_BLUE,1);
			gpio_set_value(LED_LEFT_GREEN,0);
			gpio_set_value(LED_LEFT_RED,1);
			update(1, 0,pwm_duty_purple[arg]);	
			left_pwmstatus = pwm_duty_purple[arg];
			left_ledstatus	= 6 ;				
			break;
		case PWM_SEEK_ALL_LEFT:		       
			gpio_set_value(LED_LEFT_BLUE,1);
			gpio_set_value(LED_LEFT_GREEN,1);
			gpio_set_value(LED_LEFT_RED,1);
			update(1, 0,pwm_duty_white[arg]);	
			left_pwmstatus = pwm_duty_white[arg];
            left_ledstatus	= 7 ;				
			break;

		case LED_OFF:
		    gpio_set_value(LED_LEFT_RED,0);
			gpio_set_value(LED_LEFT_BLUE,0);
			gpio_set_value(LED_LEFT_GREEN,0);
		
        	gpio_set_value(LED_RIGHT_BLUE,0);
			gpio_set_value(LED_RIGHT_GREEN,0);
			gpio_set_value(LED_RIGHT_RED,0);
			update(1, 1,pwm_duty_single[0]);		
            left_ledstatus	= 8 ;	
            right_ledstatus = 8 ;			
        break;		
		default:
		break;
    }
    return 0;
}

void waled_sleepc(int status)
{
	gpio_set_value(LED_LEFT_RED,0);
	gpio_set_value(LED_LEFT_BLUE,0);
	gpio_set_value(LED_LEFT_GREEN,0);

	gpio_set_value(LED_RIGHT_BLUE,0);
	gpio_set_value(LED_RIGHT_GREEN,0);
	gpio_set_value(LED_RIGHT_RED,0);
	update(1, 1,0x00);
	sleepstatus=1;
	status=0;
}
EXPORT_SYMBOL(waled_sleepc);


void waled_resumec(int status)
{
	status=0;
	if(left_ledstatus == 1)
	{
		gpio_set_value(LED_LEFT_RED,1);
		gpio_set_value(LED_LEFT_BLUE,0);
		gpio_set_value(LED_LEFT_GREEN,0);
		update(1, 0,left_pwmstatus);	
	}
	else if(left_ledstatus == 2)
	{
		gpio_set_value(LED_LEFT_RED,0);
		gpio_set_value(LED_LEFT_BLUE,1);
		gpio_set_value(LED_LEFT_GREEN,0);
		update(1, 0,left_pwmstatus);
	}
	else if(left_ledstatus == 3)
	{
		gpio_set_value(LED_LEFT_RED,0);
		gpio_set_value(LED_LEFT_BLUE,0);
		gpio_set_value(LED_LEFT_GREEN,1);
		update(1, 0,left_pwmstatus);
	}
	else if(left_ledstatus == 4)
	{
		gpio_set_value(LED_LEFT_BLUE,1);
		gpio_set_value(LED_LEFT_GREEN,1);
		gpio_set_value(LED_LEFT_RED,0);
		update(1, 0,left_pwmstatus);
	}
	else if(left_ledstatus == 5)
	{
		gpio_set_value(LED_LEFT_BLUE,0);
		gpio_set_value(LED_LEFT_GREEN,1);
		gpio_set_value(LED_LEFT_RED,1);
		update(1, 0,left_pwmstatus);
	}
	else if(left_ledstatus == 6)
	{
		gpio_set_value(LED_LEFT_BLUE,1);
		gpio_set_value(LED_LEFT_GREEN,0);
		gpio_set_value(LED_LEFT_RED,1);
		update(1, 0,left_pwmstatus);
	}
	else if(left_ledstatus == 7)
	{
		gpio_set_value(LED_LEFT_BLUE,1);
		gpio_set_value(LED_LEFT_GREEN,1);
		gpio_set_value(LED_LEFT_RED,1);
		update(1, 0,left_pwmstatus);	
	}
	else if(left_ledstatus == 8)
	{
		gpio_set_value(LED_LEFT_RED,0);
		gpio_set_value(LED_LEFT_BLUE,0);
		gpio_set_value(LED_LEFT_GREEN,0);
	
		gpio_set_value(LED_RIGHT_BLUE,0);
		gpio_set_value(LED_RIGHT_GREEN,0);
		gpio_set_value(LED_RIGHT_RED,0);
		update(1, 1,pwm_duty_single[0]);	
	}
	
	if(right_ledstatus == 1)
	{
		gpio_set_value(LED_RIGHT_BLUE,0);
		gpio_set_value(LED_RIGHT_GREEN,0);
		gpio_set_value(LED_RIGHT_RED,1);
		update(0, 1,right_pwmstatus);
	}
	else if(right_ledstatus == 2)
	{
		gpio_set_value(LED_RIGHT_BLUE,1);
		gpio_set_value(LED_RIGHT_GREEN,0);
		gpio_set_value(LED_RIGHT_RED,0);
		update(0, 1,right_pwmstatus);
	}
	else if(right_ledstatus == 3)
	{
		gpio_set_value(LED_RIGHT_BLUE,0);
		gpio_set_value(LED_RIGHT_GREEN,1);
		gpio_set_value(LED_RIGHT_RED,0);
		update(0, 1,right_pwmstatus);	
	}
	else if(right_ledstatus == 4)
	{
		gpio_set_value(LED_RIGHT_BLUE,1);
		gpio_set_value(LED_RIGHT_GREEN,1);
		gpio_set_value(LED_RIGHT_RED,0);
		update(0, 1,right_pwmstatus);
	}
	else if(right_ledstatus == 5)
	{
		gpio_set_value(LED_RIGHT_BLUE,0);
		gpio_set_value(LED_RIGHT_GREEN,1);
		gpio_set_value(LED_RIGHT_RED,1);
		update(0, 1,right_pwmstatus);	
	}
	else if(right_ledstatus == 6)
	{
		gpio_set_value(LED_RIGHT_BLUE,1);
		gpio_set_value(LED_RIGHT_GREEN,0);
		gpio_set_value(LED_RIGHT_RED,1);
		update(0, 1,right_pwmstatus);	
	}
	else if(right_ledstatus == 7)
	{
		gpio_set_value(LED_RIGHT_BLUE,1);
		gpio_set_value(LED_RIGHT_GREEN,1);
		gpio_set_value(LED_RIGHT_RED,1);
		update(0, 1,right_pwmstatus);
	}
	
	sleepstatus=0;

}
EXPORT_SYMBOL(waled_resumec);

static int led_open(struct inode * inode,struct file * file)
{
 	//printk("######rocky##### led open\n");
 	return 0;
}

static long led_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	ret = led_ioctl(filp, cmd, arg);
	return ret;
}

static const struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.unlocked_ioctl = led_unlocked_ioctl,
};

#endif



static int led_pwm_probe(struct platform_device *pdev)
{
#if 0	
	struct led_pwm_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct led_pwm_priv *priv;
	int count, i;
	int ret = 0;

	if (pdata)
		count = pdata->num_leds;
	else
		count = of_get_child_count(pdev->dev.of_node);

	if (!count)
		return -EINVAL;

	priv = devm_kzalloc(&pdev->dev, sizeof_pwm_leds_priv(count),
				GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	if (pdata) {
		printk("pwm two\n");
		for (i = 0; i < count; i++) {
			ret = led_pwm_add(&pdev->dev, priv, &pdata->leds[i],
					  NULL);
			if (ret)
				break;
		}
	} else {
		printk("pwm one\n");
		ret = led_pwm_create_of(&pdev->dev, priv);
	}
	

	if (ret) {
		printk("pwm add fail\n");
		led_pwm_cleanup(priv);
		return ret;
	}

	platform_set_drvdata(pdev, priv);
#endif	
	int ret;
	 ret = pwm_wa_parse_dt(pdev);
    if (ret < 0) {
        dev_err(&pdev->dev, "led_data: Device not found\n");
        return -ENODEV;
    }
	
	if ( devm_gpio_request_one(&pdev->dev, LED_LEFT_GREEN, GPIOF_DIR_OUT, "led_left_green") < 0 ){

		printk("led_left_green gpio_four request fail\n");
		gpio_free(LED_LEFT_GREEN);
		return ret;	
	}
	if ( devm_gpio_request_one(&pdev->dev, LED_LEFT_BLUE, GPIOF_DIR_OUT, "led_left_blue") < 0 ){

			printk("led_left_blue gpio_four request fail\n");
    		gpio_free(LED_LEFT_BLUE);
    		return ret;	
    	}
	
	if ( devm_gpio_request_one(&pdev->dev, LED_LEFT_RED, GPIOF_DIR_OUT, "led_left_red") < 0 ){

		printk("led_left_red gpio_four request fail\n");
		gpio_free(LED_LEFT_RED);
		return ret;	
	}
	
	
	if ( devm_gpio_request_one(&pdev->dev, LED_RIGHT_GREEN, GPIOF_DIR_OUT, "led_right_green") < 0 ){

		printk("led_right_green gpio_four request fail\n");
		gpio_free(LED_RIGHT_GREEN);
		return ret;	
	}
	if ( devm_gpio_request_one(&pdev->dev, LED_RIGHT_BLUE, GPIOF_DIR_OUT, "led_right_blue") < 0 ){

			printk("led_right_blue gpio_four request fail\n");
    		gpio_free(LED_RIGHT_BLUE);
    		return ret;	
    }
	if ( devm_gpio_request_one(&pdev->dev, LED_RIGHT_RED, GPIOF_DIR_OUT, "led_right_red") < 0 ){

			printk("led_right_red gpio_four request fail\n");
    		gpio_free(LED_RIGHT_RED);
    		return ret;	
    }
	//printk("pwm_probe   left-pwm-id is%d right-pwm-id is %d !\n",led_data.left.pwm_id,led_data.right.pwm_id);
 	//printk("pwm_probe   left-period is%d right-period is %d !\n",led_data.left.period,led_data.right.period);
 	//printk("pwm_probe   left-level is%d right-level is %d ! \n",led_data.left.level,led_data.right.level);
#if 1
	/*led_data.left.pwm = pwm_request(led_data.left.pwm_id, "pwm-left");
    if (IS_ERR(led_data.left.pwm)) {
        dev_err(&pdev->dev, "pwm: unable to request legacy PWM left\n");
        ret = PTR_ERR(led_data.left.pwm);
        return ret;
    }*/
//modify by rocky fix request error-------------------
	led_data.left.pwm = devm_pwm_get(&pdev->dev, NULL);
	if (IS_ERR(led_data.left.pwm) && PTR_ERR(led_data.left.pwm) != -EPROBE_DEFER) {
		dev_err(&pdev->dev, "unable to request PWM, trying legacy API\n");
		//pb->legacy = true;
		led_data.left.pwm = pwm_request(led_data.left.pwm_id, "pwm-left");
		printk("######rocky##### %s---%d\n",__func__,__LINE__);
	}

	if (IS_ERR(led_data.left.pwm)) {
		ret = PTR_ERR(led_data.left.pwm);
		printk("######rocky##### %s---%d\n",__func__,__LINE__);
		if (ret != -EPROBE_DEFER){
			printk("######rocky##### %s---%d\n",__func__,__LINE__);
			dev_err(&pdev->dev, "unable to request left PWM\n");
		}
		return ret;
	}
//modify by rocky fix request error-------------------
    printk("######rocky##### %s---%d\n",__func__,__LINE__);
	
    led_data.right.pwm = pwm_request(led_data.right.pwm_id, "pwm-right");
    if (IS_ERR(led_data.right.pwm)) {
        dev_err(&pdev->dev, "pwm: unable to request legacy PWM right\n");
        ret = PTR_ERR(led_data.right.pwm);
        return ret;
    }
#endif
    led_data.left.level = 10;
    led_data.right.level = 10;
    pwm_wa_update(1,1);

    // 设置极性必须在设置参数之后，pwm使能之前，否则会设置失败
    ret = pwm_set_polarity(led_data.left.pwm, PWM_POLARITY_NORMAL);
    if (ret < 0)
        printk("pwm set left polarity fail, ret = %d\n", ret);
    ret = pwm_set_polarity(led_data.right.pwm, PWM_POLARITY_NORMAL);
    if (ret < 0)
        printk("pwm set right polarity fail, ret = %d\n", ret);

    pwm_wa_enable(1,1);

    pwm_wa_disable(1,1);

#if 1
	if (register_chrdev(led_MAJOR, led_NAME, &led_fops) < 0) {
	printk(KERN_INFO "%s: Unable to get major number %d for wa-led device.\n",
	   led_NAME, led_MAJOR);
	return -1;
	}
	yf_class = class_create(THIS_MODULE, led_NAME);
	if (IS_ERR(yf_class)) {
	unregister_chrdev(led_MAJOR, "capi20");
	return PTR_ERR(yf_class);
	}
	device_create(yf_class, NULL, MKDEV(led_MAJOR, 0), NULL, "ledjni");
#endif   
   printk("led_pwm_probe success!"); 
    
/*    // 设置pwm参数
   pwm_config(wa.pwm, 500000, 1000000);

    // 设置极性必须在设置参数之后，pwm使能之前，否则会设置失败
    ret = pwm_set_polarity(wa.pwm, PWM_POLARITY_NORMAL);
    if (ret < 0)
        printk("pwm set polarity fail, ret = %d\n", ret);

    // 调试完屏蔽使能函数，交由app控制
	pwm_enable(wa.pwm);
*/
/*
	printk("pwm add probe success\n");

	pwm_config(&pdata->leds[0], 100, 1000000);

	pwm_enable(&pdata->leds[0]);
	pwm_config(&pdata->leds[1], 100, 1000000);

	pwm_enable(&pdata->leds[1]);

	gpio_set_value(LED_LEFT_BLUE,1);
	gpio_set_value(LED_LEFT_GREEN,1);
	gpio_set_value(LED_LEFT_RED,1);
*/
/*
    update(1, 0,pwm_duty[6]);
	update(0, 1,pwm_duty[6]);
	gpio_set_value(LED_LEFT_BLUE,1);
	gpio_set_value(LED_LEFT_GREEN,1);
	gpio_set_value(LED_LEFT_RED,1);
	gpio_set_value(LED_RIGHT_BLUE,1);
	gpio_set_value(LED_RIGHT_GREEN,1);
	gpio_set_value(LED_RIGHT_RED,1);
*/
	return 0;
}

static int led_pwm_remove(struct platform_device *pdev)
{
  // struct led_pwm_priv *priv = platform_get_drvdata(pdev);

//	led_pwm_cleanup(priv);

	return 0;
}
/*
static int led_pwm_suspend(struct platform_device *pdev)
{
	struct led_pwm_priv *priv = platform_get_drvdata(pdev);

    pwm_wa_disable(1,1);
    printk("led pwm suspend!");
	return 0;
}

static int led_pwm_resume(struct platform_device *pdev)
{
	 struct led_pwm_priv *priv = platform_get_drvdata(pdev);
     printk("led pwm resume!");
     pwm_wa_enable(1,1);

	return 0;
}
  
static SIMPLE_DEV_PM_OPS(led_pwm_pm, led_pwm_suspend, led_pwm_resume);
*/
   
static const struct of_device_id of_pwm_leds_match[] = {
	{ .compatible = "pwm-wa-leds", },
	{},
};
MODULE_DEVICE_TABLE(of, of_pwm_leds_match);

static struct platform_driver wa_led_pwm_driver = {
	.probe		= led_pwm_probe,
	.remove		= led_pwm_remove,
//	.suspend = led_pwm_suspend,
//	.resume = led_pwm_resume,
	.driver		= {
		.name	= "leds_wa_pwm",
//		.pm     = &led_pwm_pm ,
		.of_match_table = of_pwm_leds_match,
	},
};

module_platform_driver(wa_led_pwm_driver);

MODULE_AUTHOR("xxx");
MODULE_DESCRIPTION("PWM LED driver for wa");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:leds-wa-pwm");
