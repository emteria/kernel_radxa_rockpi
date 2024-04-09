/*
 * drivers/input/touchscreen/ft5x0x_ts.c
 *
 * FocalTech ft5x TouchScreen driver.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 *  note: only support mulititouch  Wenfs 2010-10-01
 *  for this touchscreen to work, it's slave addr must be set to 0x7e | 0x70
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>

#include <linux/interrupt.h>
#include <linux/delay.h>


#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/async.h>

#include <linux/init.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <linux/mutex.h>
#include <linux/regulator/consumer.h>


#include "ft5x_ts.h"

//---------------------------------------------

enum{
    DEBUG_INIT = 1U << 0,
    DEBUG_SUSPEND = 1U << 1,
    DEBUG_INT_INFO = 1U << 2,
    DEBUG_X_Y_INFO = 1U << 3,
    DEBUG_KEY_INFO = 1U << 4,
    DEBUG_WAKEUP_INFO = 1U << 5,
    DEBUG_OTHERS_INFO = 1U << 6,
};

static u32 debug_mask = 0xff;

#define dprintk(level_mask,fmt,arg...)    if(unlikely(debug_mask & level_mask)) \
        printk("***ft5x_ts_sunty***"fmt, ## arg)
module_param_named(debug_mask,debug_mask,int,S_IRUGO | S_IWUSR | S_IWGRP);

//---------------------------------------------

#define TOUCH_POINT_NUM (10)
//#define CONFIG_SUPPORT_FTS_CTP_UPG
//#define FOR_TSLIB_TEST
//#define TOUCH_KEY_SUPPORT
#ifdef TOUCH_KEY_SUPPORT
#define TOUCH_KEY_FOR_EVB13
//#define TOUCH_KEY_FOR_ANGDA
#ifdef TOUCH_KEY_FOR_ANGDA
#define TOUCH_KEY_X_LIMIT           (60000)
#define TOUCH_KEY_NUMBER            (4)
#endif
#ifdef TOUCH_KEY_FOR_EVB13
#define TOUCH_KEY_LOWER_X_LIMIT         (848)
#define TOUCH_KEY_HIGHER_X_LIMIT    (852)
#define TOUCH_KEY_NUMBER            (5)
#endif
#endif


//FT5X02_CONFIG
#define FT5X02_CONFIG_NAME "fttpconfig_5x02public.ini"
extern int ft5x02_Init_IC_Param(struct i2c_client * client);
extern int ft5x02_get_ic_param(struct i2c_client * client);
extern int ft5x02_Get_Param_From_Ini(char *config_name);

struct Upgrade_Info {
    u16 delay_aa;       /*delay of write FT_UPGRADE_AA */
    u16 delay_55;       /*delay of write FT_UPGRADE_55 */
    u8 upgrade_id_1;    /*upgrade id 1 */
    u8 upgrade_id_2;    /*upgrade id 2 */
    u16 delay_readid;   /*delay of read id */
};

static struct i2c_client *this_client;

#ifdef TOUCH_KEY_SUPPORT
static int key_tp  = 0;
static int key_val = 0;
#endif

/*********************************************************************************************/
#define CTP_NAME             "ft5x_ts_sunty"

#define DEFAULT_SCREEN_MAX_X            (1920)
#define DEFAULT_SCREEN_MAX_Y            (1080)
#define PRESS_MAX           (255)


/* -- dirver configure -- */
#define CFG_MAX_TOUCH_POINTS    10//5

#define FT_PRESS    0x08

#define FT_MAX_ID   0x0F
#define FT_TOUCH_STEP   6
#define FT_TOUCH_POINT_NUM      2
#define FT_TOUCH_X_H_POS        3
#define FT_TOUCH_X_L_POS        4
#define FT_TOUCH_Y_H_POS        5
#define FT_TOUCH_Y_L_POS        6
#define FT_TOUCH_XY_POS         7
#define FT_TOUCH_EVENT_POS      3
#define FT_TOUCH_ID_POS         5

#define POINT_READ_BUF  (3 + FT_TOUCH_STEP * CFG_MAX_TOUCH_POINTS)

/*register address*/
#define FT5x0x_REG_FW_VER       0xA6
#define FT5x0x_REG_POINT_RATE   0x88
#define FT5X0X_REG_THGROUP  0x80


/*********************************************************************************************/
/*------------------------------------------------------------------------------------------*/
/* Addresses to scan */
static const unsigned short normal_i2c[2] = {0x38,I2C_CLIENT_END};
static const int chip_id_value[] = {0x55,0x06,0x08,0x02,0xa3};
static int chip_id = 0;

/*------------------------------------------------------------------------------------------*/

int fts_ctpm_fw_upgrade_with_i_file(struct i2c_client *client);

static int ft5x_i2c_rxdata(char *rxdata, int length);

int cmd_write(u8 btcmd,u8 btPara1,u8 btPara2,u8 btPara3,u8 num);

int hidi2c_to_stdi2c(struct i2c_client * client);

int ft5x02_i2c_Read(struct i2c_client *client,  char * writebuf, int writelen, char *readbuf, int readlen);

struct ts_event {
    u16 au16_x[CFG_MAX_TOUCH_POINTS];   /*x coordinate */
    u16 au16_y[CFG_MAX_TOUCH_POINTS];   /*y coordinate */
    u8 au8_touch_event[CFG_MAX_TOUCH_POINTS];   /*touch event:
                    0 -- down; 1-- up; 2 -- contact */
    u8 au8_finger_id[CFG_MAX_TOUCH_POINTS]; /*touch ID */
    u8 au8_xy[CFG_MAX_TOUCH_POINTS];
    u16 pressure;
    u8 touch_point;
    int touchs;
    u8 touch_point_num;

};

struct ft5x_ts_data {
    struct i2c_client *client;
    struct input_dev *input_dev;

    struct workqueue_struct *ts_workqueue;
    struct work_struct pen_event_work;
    struct work_struct init_events_work;
    struct work_struct resume_events_work;

    struct ts_event event;

    //dts node : power-supply
    //supply = devm_regulator_get(dev, "power");
    struct regulator *supply;
    struct gpio_desc *enable_gpio; //dts node: enable-gpios
    struct gpio_desc *reset_gpio; //dts node: reset-gpios
    struct gpio_desc *irq_gpio;// dts node: irq-gpios
    int irq;
    bool irq_enabled;
    struct mutex mutex_lock;

    /*
    dts node:
        screen_max_x = <1536>;
        screen_max_y = <2048>;
        exchange_x_y_flag = <1>;
        revert_x_flag = <0>;
        revert_y_flag = <0>;
    */
    int screen_max_x;
    int screen_max_y;
    int revert_x_flag;
    int revert_y_flag;
    int exchange_x_y_flag;

};

//for we use  devm_gpiod_xxx
#define ENABLE_GPIO_ACTIVE_VALUE (1)
#define RESET_GPIO_ACTIVE_VALUE (0)
#define IRQ_GPIO_ACTIVE_VALUE (0)

struct ft5x_ts_data *g_data;

/* ---------------------------------------------------------------------
*
*   Focal Touch panel upgrade related driver
*
*
----------------------------------------------------------------------*/

typedef enum
{
    ERR_OK,
    ERR_MODE,
    ERR_READID,
    ERR_ERASE,
    ERR_STATUS,
    ERR_ECC,
    ERR_DL_ERASE_FAIL,
    ERR_DL_PROGRAM_FAIL,
    ERR_DL_VERIFY_FAIL
}E_UPGRADE_ERR_TYPE;

typedef unsigned char         FTS_BYTE;     //8 bit
typedef unsigned short        FTS_WORD;    //16 bit
typedef unsigned int          FTS_DWRD;    //16 bit
typedef unsigned char         FTS_BOOL;    //8 bit

#define FTS_NULL                0x0
#define FTS_TRUE                0x01
#define FTS_FALSE               0x0

#define I2C_CTPM_ADDRESS        (0x70>>1)

static void delay_ms(FTS_WORD  w_ms)
{
    //platform related, please implement this function
    msleep( w_ms );
}

static void delay_qt_ms(unsigned long  w_ms)
{
    unsigned long i;
    unsigned long j;

    for (i = 0; i < w_ms; i++)
    {
        for (j = 0; j < 1000; j++)
        {
             udelay(1);
        }
    }
}

/************************************************************************
* Name: hidi2c_to_stdi2c
* Brief:  HID to I2C
* Input: i2c info
* Output: no
* Return: fail =0
***********************************************************************/
int hidi2c_to_stdi2c(struct i2c_client * client)
{
	u8 auc_i2c_write_buf[5] = {0};
	int bRet = 0;
/*	#if HIDTOI2C_DISABLE	
		return 0;
	#endif*/
	auc_i2c_write_buf[0] = 0xeb;
	auc_i2c_write_buf[1] = 0xaa;
	auc_i2c_write_buf[2] = 0x09;
	bRet = cmd_write(0xeb,0xaa,0x09,0x00,3);
	msleep(10);
	auc_i2c_write_buf[0] = auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = 0;
	ft5x02_i2c_Read(client, auc_i2c_write_buf, 0, auc_i2c_write_buf, 3);

	if(0xeb==auc_i2c_write_buf[0] && 0xaa==auc_i2c_write_buf[1] && 0x08==auc_i2c_write_buf[2])
	{
		//pr_info("hidi2c_to_stdi2c successful.\n");
		bRet = 1;		
	}
	else 
	{
		//pr_err("hidi2c_to_stdi2c error.\n");
		bRet = 0;
	}

	return bRet;
}

/*
*ft5x02_i2c_Read-read data and write data by i2c
*@client: handle of i2c
*@writebuf: Data that will be written to the slave
*@writelen: How many bytes to write
*@readbuf: Where to store data read from slave
*@readlen: How many bytes to read
*
*Returns negative errno, else the number of messages executed
*
*
*/
int ft5x02_i2c_Read(struct i2c_client *client,  char * writebuf, int writelen,
							char *readbuf, int readlen)
{
	int ret;

	if(writelen > 0) {
		struct i2c_msg msgs[] = {
			{
				.addr	= client->addr,
				.flags	= 0,
				.len	= writelen,
				.buf	= writebuf,
			},
			{
				.addr	= client->addr,
				.flags	= I2C_M_RD,
				.len	= readlen,
				.buf	= readbuf,
			},
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			pr_err("function:%s. i2c read error: %d\n", __func__, ret);
	}
	else{
		struct i2c_msg msgs[] = {
			{
				.addr	= client->addr,
				.flags	= I2C_M_RD,
				.len	= readlen,
				.buf	= readbuf,
			},
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			pr_err("function:%s. i2c read error: %d\n", __func__, ret);
	}
	return ret;
}
/*
*write data by i2c
*/
int ft5x02_i2c_Write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= writelen,
			.buf	= writebuf,
		},
	};

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0)
		pr_err("%s i2c write error: %d\n", __func__, ret);

	return ret;
}



static int ft5x_reset(struct ft5x_ts_data *data)
{
    if (!data) return -ENOMEM;

    if (!data->reset_gpio) return -ENOMEM;

    gpiod_set_value(data->reset_gpio, RESET_GPIO_ACTIVE_VALUE);
    delay_ms(40);
    gpiod_set_value(data->reset_gpio, !RESET_GPIO_ACTIVE_VALUE);
    delay_ms(40);

    return 0;
}

static int ft5x_set_power_enabled(struct ft5x_ts_data *data, bool enabled)
{
    int ret = 0;
    if (!data) return -ENOMEM;

    if (enabled) {
        if (data->supply) {
            ret = regulator_enable(data->supply);
            if (!ret) {
               ;// dprintk(DEBUG_OTHERS_INFO, "%s[%d]: OK to regulator_enable", __func__, __LINE__);
            } else {
                ;//dprintk(DEBUG_OTHERS_INFO, "%s[%d]: Fail to regulator_enable", __func__, __LINE__);
            }
        }
        if (data->enable_gpio) {
            gpiod_set_value(data->enable_gpio, ENABLE_GPIO_ACTIVE_VALUE);
        }
    } else {
        if (data->supply) {
            ret = regulator_disable(data->supply);
        }
        if (data->enable_gpio) {
            gpiod_set_value(data->enable_gpio, !ENABLE_GPIO_ACTIVE_VALUE);
        }
    }

    return ret;
}

static void ft5x_set_irq_enabled(struct ft5x_ts_data *data, bool enabled)
{
    bool en;
    if (!data) return;
    if (!(data->irq_gpio && data->irq > 0)) return;


	mutex_lock(&data->mutex_lock);
	en = !!enabled;
	//dprintk(DEBUG_INT_INFO, "%s, irq_enabled = %d, en = %d\n", __func__, data->irq_enabled, en);
	if (data->irq_enabled == en) {
		goto _out;
	}
	data->irq_enabled = en;

	if (en) {
		enable_irq(data->irq);
	} else {
		disable_irq(data->irq);
	}

_out:
	mutex_unlock(&data->mutex_lock);
}


/*
[function]:
    callback: read data from ctpm by i2c interface,implemented by special user;
[parameters]:
    bt_ctpm_addr[in]    :the address of the ctpm;
    pbt_buf[out]        :data buffer;
    dw_lenth[in]        :the length of the data buffer;
[return]:
    FTS_TRUE     :success;
    FTS_FALSE    :fail;
*/
static int i2c_read_interface(u8 bt_ctpm_addr, u8* pbt_buf, u16 dw_lenth)
{
    int ret;

    ret = i2c_master_recv(this_client, pbt_buf, dw_lenth);

    if(ret != dw_lenth){
        printk("ret = %d. \n", ret);
        printk("i2c_read_interface error\n");
        return FTS_FALSE;
    }

    return FTS_TRUE;
}

/*
[function]:
    callback: write data to ctpm by i2c interface,implemented by special user;
[parameters]:
    bt_ctpm_addr[in]    :the address of the ctpm;
    pbt_buf[in]        :data buffer;
    dw_lenth[in]        :the length of the data buffer;
[return]:
    FTS_TRUE     :success;
    FTS_FALSE    :fail;
*/
int i2c_write_interface(u8 bt_ctpm_addr, u8* pbt_buf, u16 dw_lenth)
{
    int ret;
    ret=i2c_master_send(this_client, pbt_buf, dw_lenth);
    if(ret != dw_lenth){
        printk("i2c_write_interface error\n");
        return FTS_FALSE;
    }

    return FTS_TRUE;
}


/***************************************************************************************/

/*
[function]:
    read out the register value.
[parameters]:
    e_reg_name[in]    :register name;
    pbt_buf[out]    :the returned register value;
    bt_len[in]        :length of pbt_buf, should be set to 2;
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
u8 fts_register_read(u8 e_reg_name, u8* pbt_buf, u8 bt_len)
{
    u8 read_cmd[3]= {0};
    u8 cmd_len     = 0;

    read_cmd[0] = e_reg_name;
    cmd_len = 1;

    /*call the write callback function*/
    //    if(!i2c_write_interface(I2C_CTPM_ADDRESS, &read_cmd, cmd_len))
    //    {
    //        return FTS_FALSE;
    //    }


    if(!i2c_write_interface(I2C_CTPM_ADDRESS, read_cmd, cmd_len))   {//change by zhengdixu
        return FTS_FALSE;
    }

    /*call the read callback function to get the register value*/
    if(!i2c_read_interface(I2C_CTPM_ADDRESS, pbt_buf, bt_len)){
        return FTS_FALSE;
    }
    return FTS_TRUE;
}

/*
[function]:
    write a value to register.
[parameters]:
    e_reg_name[in]    :register name;
    pbt_buf[in]        :the returned register value;
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
int fts_register_write(u8 e_reg_name, u8 bt_value)
{
    FTS_BYTE write_cmd[2] = {0};

    write_cmd[0] = e_reg_name;
    write_cmd[1] = bt_value;

    /*call the write callback function*/
    //return i2c_write_interface(I2C_CTPM_ADDRESS, &write_cmd, 2);
    return i2c_write_interface(I2C_CTPM_ADDRESS, write_cmd, 2); //change by zhengdixu
}

/*
[function]:
    send a command to ctpm.
[parameters]:
    btcmd[in]        :command code;
    btPara1[in]    :parameter 1;
    btPara2[in]    :parameter 2;
    btPara3[in]    :parameter 3;
    num[in]        :the valid input parameter numbers, if only command code needed and no parameters followed,then the num is 1;
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
int cmd_write(u8 btcmd,u8 btPara1,u8 btPara2,u8 btPara3,u8 num)
{
    FTS_BYTE write_cmd[4] = {0};

    write_cmd[0] = btcmd;
    write_cmd[1] = btPara1;
    write_cmd[2] = btPara2;
    write_cmd[3] = btPara3;
    //return i2c_write_interface(I2C_CTPM_ADDRESS, &write_cmd, num);
    return i2c_write_interface(I2C_CTPM_ADDRESS, write_cmd, num);//change by zhengdixu
}

/*
[function]:
    write data to ctpm , the destination address is 0.
[parameters]:
    pbt_buf[in]    :point to data buffer;
    bt_len[in]        :the data numbers;
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
int byte_write(u8* pbt_buf, u16 dw_len)
{
    return i2c_write_interface(I2C_CTPM_ADDRESS, pbt_buf, dw_len);
}

/*
[function]:
    read out data from ctpm,the destination address is 0.
[parameters]:
    pbt_buf[out]    :point to data buffer;
    bt_len[in]        :the data numbers;
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
int byte_read(u8* pbt_buf, u8 bt_len)
{
    return i2c_read_interface(I2C_CTPM_ADDRESS, pbt_buf, bt_len);
    //ft5x_i2c_rxdata
}


/*
[function]:
    burn the FW to ctpm.
[parameters]:(ref. SPEC)
    pbt_buf[in]    :point to Head+FW ;
    dw_lenth[in]:the length of the FW + 6(the Head length);
    bt_ecc[in]    :the ECC of the FW
[return]:
    ERR_OK        :no error;
    ERR_MODE    :fail to switch to UPDATE mode;
    ERR_READID    :read id fail;
    ERR_ERASE    :erase chip fail;
    ERR_STATUS    :status error;
    ERR_ECC        :ecc error.
*/


#define    FTS_PACKET_LENGTH       128 //2//4//8//16//32//64//128//256

static unsigned char CTPM_FW[]=
{
        #include "ft_app.i"
};
unsigned char fts_ctpm_get_i_file_ver(void)
{
        u16 ui_sz;
        ui_sz = sizeof(CTPM_FW);
        if (ui_sz > 2){
              //  return CTPM_FW[ui_sz - 2];
               return CTPM_FW[0x1D0A];
        }else{
                //TBD, error handling?
                return 0xff; //default value
        }
}

/*
*get upgrade information depend on the ic type
*/
static void fts_get_upgrade_info(struct Upgrade_Info *upgrade_info)
{
    switch (chip_id) {
    case 0x55:    //IC_FT5X06:
        upgrade_info->delay_55 = FT5X06_UPGRADE_55_DELAY;
        upgrade_info->delay_aa = FT5X06_UPGRADE_AA_DELAY;
        upgrade_info->upgrade_id_1 = FT5X06_UPGRADE_ID_1;
        upgrade_info->upgrade_id_2 = FT5X06_UPGRADE_ID_2;
        upgrade_info->delay_readid = FT5X06_UPGRADE_READID_DELAY;
        break;
    case 0x08:    //IC_FT5606或者IC_FT5506
        upgrade_info->delay_55 = FT5606_UPGRADE_55_DELAY;
        upgrade_info->delay_aa = FT5606_UPGRADE_AA_DELAY;
        upgrade_info->upgrade_id_1 = FT5606_UPGRADE_ID_1;
        upgrade_info->upgrade_id_2 = FT5606_UPGRADE_ID_2;
        upgrade_info->delay_readid = FT5606_UPGRADE_READID_DELAY;
        break;
    case 0x00:    //IC FT5316
    case 0x0a:    //IC FT5316
        upgrade_info->delay_55 = FT5316_UPGRADE_55_DELAY;
        upgrade_info->delay_aa = FT5316_UPGRADE_AA_DELAY;
        upgrade_info->upgrade_id_1 = FT5316_UPGRADE_ID_1;
        upgrade_info->upgrade_id_2 = FT5316_UPGRADE_ID_2;
        upgrade_info->delay_readid = FT5316_UPGRADE_READID_DELAY;
		break;
     case 0x58:    //IC FT5726
        upgrade_info->delay_55 = FT5726_UPGRADE_55_DELAY;
        upgrade_info->delay_aa = FT5726_UPGRADE_AA_DELAY;
        upgrade_info->upgrade_id_1 = FT5726_UPGRADE_ID_1;
        upgrade_info->upgrade_id_2 = FT5726_UPGRADE_ID_2;
        upgrade_info->delay_readid = FT5726_UPGRADE_READID_DELAY;
        break;
    default:
        break;
    }
}

E_UPGRADE_ERR_TYPE  ft5x06_ctpm_fw_upgrade(struct i2c_client *client,u8* pbt_buf, u16 dw_lenth)
{
    u8 reg_val[2] = {0};
    FTS_BOOL i_ret = 0;
    u16 i = 0;


    u16  packet_number;
    u16  j;
    u16  temp;
    u16  lenght;
    u8  packet_buf[FTS_PACKET_LENGTH + 6];
    u8  auc_i2c_write_buf[10];
    u8 bt_ecc;
    u8 bt_ecc_check;
   
   //struct i2c_client *client;
   struct  Upgrade_Info upgradeinfo = {0, 0, 0, 0 , 0};
    fts_get_upgrade_info(&upgradeinfo);
    i_ret = hidi2c_to_stdi2c(client);
	if (i_ret == 0) 
	{
		//FTS_DBG("HidI2c change to StdI2c fail ! \n");
	}
	for (i = 0; i < 30; i++) 
	{
    /*********Step 1:Reset  CTPM *****/
    /*write 0xaa to register 0xfc*/
    //delay_ms(100);//最新的源码去掉延时
    fts_register_write(0xfc,0xaa);
    delay_ms(upgradeinfo.delay_aa);

    /*write 0x55 to register 0xfc*/
    fts_register_write(0xfc,0x55);
    printk("Step 1: Reset CTPM test\n");
    //delay_ms(upgradeinfo.delay_55);
    delay_ms(200);
    /*********Step 2:Enter upgrade mode *****/
	i_ret = hidi2c_to_stdi2c(client);
		if (i_ret == 0) 
		{
		//	FTS_DBG("HidI2c change to StdI2c fail ! \n");
		}
		msleep(5);
		cmd_write(0x55,0xaa,0x00,0x00,2);
  //  auc_i2c_write_buf[0] = 0x55;
 //   auc_i2c_write_buf[1] = 0xaa;
 	
 /*  i = 0;
    do{
            i++;
            //i_ret = i2c_write_interface(I2C_CTPM_ADDRESS, auc_i2c_write_buf, 2);
    cmd_write(0x55,0xaa,0x00,0x00,2);
            printk("Step 2: Enter update mode. \n");
            delay_ms(5);
    }while((FTS_FALSE == i_ret) && i<5);*/

    /*********Step 3:check READ-ID***********************/
    /*send the opration head*/
    msleep(upgradeinfo.delay_readid);
    cmd_write(0x90,0x00,0x00,0x00,4);
    byte_read(reg_val,2);
	printk("\n Jessica4 %x||%x__%x__%x \n",reg_val[0],reg_val[1],upgradeinfo.upgrade_id_1,upgradeinfo.upgrade_id_2);
    //if (reg_val[0] == upgradeinfo.upgrade_id_1&& reg_val[1] == upgradeinfo.upgrade_id_2) {
	if (reg_val[0] == 0x58&& reg_val[1] == 0x2c) {
        printk("Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
		break;
    }
    else {
        printk("Step 3_2: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
        return ERR_READID;
    }
		}
  if (i >= 30)
		return -EIO;
   // cmd_write(0xcd,0x00,0x00,0x00,1);
   // byte_read(reg_val,1);

    /*Step 4:erase app and panel paramenter area*/
    cmd_write(0x61,0x00,0x00,0x00,1);
    msleep(1350);
  //  cmd_write(0x63,0x00,0x00,0x00,1);
  //  msleep(100);
  //  printk("Step 4: erase. \n");
  for (i = 0; i < 15; i++) 
	{
		auc_i2c_write_buf[0] = 0x6a;
		reg_val[0] = reg_val[1] = 0x00;
		ft5x02_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 2);
		if (0xF0 == reg_val[0] && 0xAA == reg_val[1]) 
		{
			break;
		}
		msleep(50);
	}
	printk("[FTS][%s] erase app area reg_val[0] = %x reg_val[1] = %x \n", __func__, reg_val[0], reg_val[1]);
	auc_i2c_write_buf[0] = 0xB0;
	auc_i2c_write_buf[1] = (u8) ((dw_lenth >> 16) & 0xFF);
	auc_i2c_write_buf[2] = (u8) ((dw_lenth >> 8) & 0xFF);
	auc_i2c_write_buf[3] = (u8) (dw_lenth & 0xFF);
	ft5x02_i2c_Write(client, auc_i2c_write_buf, 4);
    /*********Step 5:write firmware(FW) to ctpm flash*********/
    bt_ecc = 0;
    bt_ecc_check = 0;
    printk("Step 5: start upgrade. \n");
   // dw_lenth = dw_lenth - 8;
    temp = 0;
    packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
    packet_buf[0] = 0xbf;
    packet_buf[1] = 0x00;
    for (j = 0; j < packet_number; j++){
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(lenght>>8);
        packet_buf[5] = (FTS_BYTE)lenght;
        for (i=0;i<FTS_PACKET_LENGTH;i++){
                packet_buf[6+i] = pbt_buf[j*FTS_PACKET_LENGTH + i];
		   bt_ecc_check ^= pbt_buf[j * FTS_PACKET_LENGTH + i];
                bt_ecc ^= packet_buf[6+i];
        }
        printk("[FTS][%s] bt_ecc = %x \n", __func__, bt_ecc);
	 if (bt_ecc != bt_ecc_check)
		printk("[FTS][%s] Host checksum error bt_ecc_check = %x \n", __func__, bt_ecc_check);
         ft5x02_i2c_Write(client, packet_buf, FTS_PACKET_LENGTH + 6);
	   // byte_write(&packet_buf[0],FTS_PACKET_LENGTH + 6);
        //delay_ms(FTS_PACKET_LENGTH/6 + 1);
 /*       msleep(FTS_PACKET_LENGTH/6 + 1);
        if ((j * FTS_PACKET_LENGTH % 1024) == 0){
                printk("upgrade the 0x%x th byte.\n", ((unsigned int)j) * FTS_PACKET_LENGTH);
        }*/
        for (i = 0; i < 30; i++) 
		{
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			ft5x02_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 2);
			if ((j + 0x1000) == (((reg_val[0]) << 8) | reg_val[1])) {
				break;
			}
			printk("[FTS][%s] reg_val[0] = %x reg_val[1] = %x \n", __func__, reg_val[0], reg_val[1]);
			msleep(1);
		}
    }
    if ((dw_lenth) % FTS_PACKET_LENGTH > 0){
        temp = packet_number * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;

        temp = (dw_lenth) % FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(temp>>8);
        packet_buf[5] = (FTS_BYTE)temp;

        for (i=0;i<temp;i++){
                packet_buf[6+i] = pbt_buf[ packet_number*FTS_PACKET_LENGTH + i];
                bt_ecc_check ^= pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
		   bt_ecc ^= packet_buf[6+i];
        }
        ft5x02_i2c_Write(client, packet_buf, temp + 6);
	printk("[FTS][%s] bt_ecc = %x \n", __func__, bt_ecc);
		if (bt_ecc != bt_ecc_check)
			printk("[FTS][%s] Host checksum error bt_ecc_check = %x \n", __func__, bt_ecc_check);
    //    byte_write(&packet_buf[0],temp+6);
        //delay_ms(20);
     //   msleep(20);
     for (i = 0; i < 30; i++) 
		{
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			ft5x02_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 2);
			printk("[FTS][%s] reg_val[0] = %x reg_val[1] = %x \n", __func__, reg_val[0], reg_val[1]);
			if ((j + 0x1000) == (((reg_val[0]) << 8) | reg_val[1])) 
			{
				break;
			}
			printk("[FTS][%s] reg_val[0] = %x reg_val[1] = %x \n", __func__, reg_val[0], reg_val[1]);
			msleep(1);
		}
    }
msleep(50);
    //send the last six byte
/*    for (i = 0; i<6; i++){
        temp = 0x6ffa + i;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        temp =1;
        packet_buf[4] = (FTS_BYTE)(temp>>8);
        packet_buf[5] = (FTS_BYTE)temp;
        packet_buf[6] = pbt_buf[ dw_lenth + i];
        bt_ecc ^= packet_buf[6];

        byte_write(&packet_buf[0],7);
        //delay_ms(20);
        msleep(20);
    }*/

    /*********Step 6: read out checksum***********************/
    /*send the opration head*/
    //cmd_write(0xcc,0x00,0x00,0x00,1);//把0xcc当作寄存器地址，去读出一个字节
    // byte_read(reg_val,1);//change by zhengdixu

  /*  fts_register_read(0xcc, reg_val,1);
    printk("Step 6:  ecc read 0x%x, new firmware 0x%x. \n", reg_val[0], bt_ecc);
    if(reg_val[0] != bt_ecc){
            //cmd_write(0x07,0x00,0x00,0x00,1);
    printk("ecc error! \n");
    return ERR_ECC;
    }*/
    auc_i2c_write_buf[0] = 0x64;
	ft5x02_i2c_Write(client, auc_i2c_write_buf, 1);
	msleep(300);
	temp = 0;
	auc_i2c_write_buf[0] = 0x65;
	auc_i2c_write_buf[1] = (u8)(temp >> 16);
	auc_i2c_write_buf[2] = (u8)(temp >> 8);
	auc_i2c_write_buf[3] = (u8)(temp);
	temp = dw_lenth;
	auc_i2c_write_buf[4] = (u8)(temp >> 8);
	auc_i2c_write_buf[5] = (u8)(temp);
	i_ret = ft5x02_i2c_Write(client, auc_i2c_write_buf, 6);
	msleep(dw_lenth/256);
	for (i = 0; i < 100; i++) 
	{
		auc_i2c_write_buf[0] = 0x6a;
		reg_val[0] = reg_val[1] = 0x00;
		ft5x02_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 2);
		dev_err(&client->dev, "[FTS]--reg_val[0]=%02x reg_val[0]=%02x\n", reg_val[0], reg_val[1]);
		if (0xF0 == reg_val[0] && 0x55 == reg_val[1]) 
		{
			dev_err(&client->dev, " [FTS]--reg_val[0]=%02x reg_val[0]=%02x\n", reg_val[0], reg_val[1]);
			break;
		}
		msleep(1);
	}
	auc_i2c_write_buf[0] = 0x66;
	ft5x02_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);
	if (reg_val[0] != bt_ecc) 
	{
		dev_err(&client->dev, "[FTS]--ecc error! FW=%02x bt_ecc=%02x\n", reg_val[0], bt_ecc);
		return -EIO;
	}
	printk(KERN_WARNING " checksum %X %X \n", reg_val[0], bt_ecc);

    /*********Step 7: reset the new FW***********************/
    cmd_write(0x07,0x00,0x00,0x00,1);
    msleep(200);
	i_ret = hidi2c_to_stdi2c(client);
      if (i_ret == 0) 
	{
		printk("HidI2c change to StdI2c fail ! \n");
	}
    return 0;
    //return ERR_OK;
}

E_UPGRADE_ERR_TYPE  ft5x02_ctpm_fw_upgrade(u8* pbt_buf, u32 dw_lenth)
{

    u8 reg_val[2] = {0};
    u32 i = 0;

    u32  packet_number;
    u32  j;
    u32  temp;
    u32  lenght;
    u8  packet_buf[FTS_PACKET_LENGTH + 6];
    //u8    auc_i2c_write_buf[10];
    u8  bt_ecc;

    //struct timeval begin_tv, end_tv;
    //do_gettimeofday(&begin_tv);

    for (i=0; i<16; i++) {
        /*********Step 1:Reset  CTPM *****/
        /*write 0xaa to register 0xfc*/
        fts_register_write(0xfc,0xaa);
        msleep(30);
        /*write 0x55 to register 0xfc*/
        fts_register_write(0xfc,0x55);
        //delay_qt_ms(18);
        delay_qt_ms(25);
        /*********Step 2:Enter upgrade mode *****/
        #if 0
        //auc_i2c_write_buf[0] = 0x55;
        //auc_i2c_write_buf[1] = 0xaa;
        do
        {
            i ++;
            //i_ret = ft5x02_i2c_Write(client, auc_i2c_write_buf, 2);
            //i_ret = i2c_write_interface(I2C_CTPM_ADDRESS, auc_i2c_write_buf, 2);
            cmd_write(0x55,0xaa,0x00,0x00,2);
            delay_qt_ms(5);
        }while(i_ret <= 0 && i < 5 );
        #else
        //auc_i2c_write_buf[0] = 0x55;
        //ft5x02_i2c_Write(client, auc_i2c_write_buf, 1);
        cmd_write(0x55,0x00,0x00,0x00,1);
        delay_qt_ms(1);
        //auc_i2c_write_buf[0] = 0xaa;
        //ft5x02_i2c_Write(client, auc_i2c_write_buf, 1);
        cmd_write(0xaa,0x00,0x00,0x00,1);
        #endif

        /*********Step 3:check READ-ID***********************/
        delay_qt_ms(1);

        //ft5x02_upgrade_send_head(client);
        cmd_write(0xFA,0xFA,0x00,0x00,2);//ft5x02_upgrade_send_head
        //auc_i2c_write_buf[0] = 0x90;
        //auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;
        //ft5x02_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);
        cmd_write(0x90,0x00,0x00,0x00,4);
        byte_read(reg_val,2);

        if (reg_val[0] == 0x79
            && reg_val[1] == 0x02) {
            //dev_dbg(&client->dev, "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
            printk("[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
            break;
        } else {
            printk("[FTS] Step 3 ERROR: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
            //delay_qt_ms(1);
        }
    }
    if (i >= 6)
        return ERR_READID;
    /********Step 4:enable write function*/
    //ft5x02_upgrade_send_head(client);
    cmd_write(0xFA,0xFA,0x00,0x00,2);//ft5x02_upgrade_send_head
    //auc_i2c_write_buf[0] = 0x06;
    //ft5x02_i2c_Write(client, auc_i2c_write_buf, 1);
    cmd_write(0x06,0x00,0x00,0x00,1);

    /*********Step 5:write firmware(FW) to ctpm flash*********/
    bt_ecc = 0;

    packet_number = (dw_lenth) / FTS_PACKET_LENGTH;

    packet_buf[0] = 0xbf;
    packet_buf[1] = 0x00;
    for (j=0; j<packet_number; j++) {
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (u8)(temp>>8);
        packet_buf[3] = (u8)temp;
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (u8)(lenght>>8);
        packet_buf[5] = (u8)lenght;
        if(temp>=0x4c00 && temp <(0x4c00+512))
            continue;

        for (i=0; i<FTS_PACKET_LENGTH; i++) {
            packet_buf[6+i] = pbt_buf[j*FTS_PACKET_LENGTH + i];
            bt_ecc ^= packet_buf[6+i];
        }
        //ft5x02_upgrade_send_head(client);
        cmd_write(0xFA,0xFA,0x00,0x00,2);//ft5x02_upgrade_send_head
        //ft5x02_i2c_Write(client, packet_buf, FTS_PACKET_LENGTH+6);
        byte_write(&packet_buf[0],FTS_PACKET_LENGTH + 6);
        delay_qt_ms(2);
    }

    if ((dw_lenth) % FTS_PACKET_LENGTH > 0) {
        temp = packet_number * FTS_PACKET_LENGTH;
        packet_buf[2] = (u8)(temp>>8);
        packet_buf[3] = (u8)temp;

        temp = (dw_lenth) % FTS_PACKET_LENGTH;
        packet_buf[4] = (u8)(temp>>8);
        packet_buf[5] = (u8)temp;

        for (i=0; i<temp; i++) {
            packet_buf[6+i] = pbt_buf[ packet_number*FTS_PACKET_LENGTH + i];
            bt_ecc ^= packet_buf[6+i];
        }
        //ft5x02_upgrade_send_head(client);
        cmd_write(0xFA,0xFA,0x00,0x00,2);//ft5x02_upgrade_send_head
        //ft5x02_i2c_Write(client, packet_buf, temp+6);
        byte_write(&packet_buf[0],temp + 6);
        delay_qt_ms(2);
    }

    /********Disable write function*/
    //ft5x02_upgrade_send_head(client);
    cmd_write(0xFA,0xFA,0x00,0x00,2);//ft5x02_upgrade_send_head
    //auc_i2c_write_buf[0] = 0x04;
    //ft5x02_i2c_Write(client, auc_i2c_write_buf, 1);
    cmd_write(0x04,0x00,0x00,0x00,1);

    delay_qt_ms(1);
    /*********Step 6: read out checksum***********************/
    //ft5x02_upgrade_send_head(client);
    cmd_write(0xFA,0xFA,0x00,0x00,2);//ft5x02_upgrade_send_head
    //auc_i2c_write_buf[0] = 0xcc;
    //ft5x02_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);
    cmd_write(0xcc,0x00,0x00,0x00,1);
    byte_read(reg_val,1);

    if (reg_val[0] != bt_ecc) {
        printk("[FTS]--ecc error! FW=%02x bt_ecc=%02x\n", reg_val[0], bt_ecc);
        //return -EIO;
        return ERR_READID;
    }

    /*********Step 7: reset the new FW***********************/
    //ft5x02_upgrade_send_head(client);
    cmd_write(0xFA,0xFA,0x00,0x00,2);//ft5x02_upgrade_send_head
    //auc_i2c_write_buf[0] = 0x07;
    //ft5x02_i2c_Write(client, auc_i2c_write_buf, 1);
    cmd_write(0x07,0x00,0x00,0x00,1);
    msleep(200);  /*make sure CTP startup normally*/
    //DBG("-------upgrade successful-----\n");

    //do_gettimeofday(&end_tv);
    //DBG("cost time=%lu.%lu\n", end_tv.tv_sec-begin_tv.tv_sec,
    //      end_tv.tv_usec-begin_tv.tv_usec);
    return ERR_OK;
}

int fts_ctpm_auto_clb(void)
{
    unsigned char uc_temp;
    unsigned char i ;

    printk("[FTS] start auto CLB.\n");
    msleep(200);
    fts_register_write(0, 0x40);
    //delay_ms(100);                       //make sure already enter factory mode
    msleep(100);
    fts_register_write(2, 0x4);               //write command to start calibration
    //delay_ms(300);
    msleep(300);
    for(i=0;i<100;i++){
            fts_register_read(0,&uc_temp,1);
            if (((uc_temp&0x70)>>4) == 0x0){    //return to normal mode, calibration finish
                    break;
            }
            //delay_ms(200);
    msleep(200);
            printk("[FTS] waiting calibration %d\n",i);
    }

    printk("[FTS] calibration OK.\n");

    msleep(300);
    fts_register_write(0, 0x40);          //goto factory mode
    delay_ms(100);                       //make sure already enter factory mode
    fts_register_write(2, 0x5);          //store CLB result
    delay_ms(300);
    fts_register_write(0, 0x0);          //return to normal mode
    msleep(300);
    printk("[FTS] store CLB result OK.\n");
    return 0;
}

void getVerNo(u8* buf, int len)
{
    u8 start_reg = FT5x0x_REG_FW_VER;
    int ret = -1;
    int i = 0;

    ret = fts_register_read(start_reg, buf, len);
    //et = ft5406_read_regs(ft5x0x_ts_data_test->client,start_reg, buf, 2);
    if (ret < 0) {
        printk("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
        return;
    }
    for (i = 0; i < 2; i++) {
        printk("=========buf[%d] = 0x%x \n", i, buf[i]);
    }
    return;
}

int fts_ctpm_fw_upgrade_with_i_file(struct i2c_client *client)
{
    FTS_BYTE*     pbt_buf = FTS_NULL;
    int i_ret = 0;
    unsigned char a;
    unsigned char b;
#define BUFFER_LEN (2)            //len == 2
    unsigned char buf[BUFFER_LEN] = {0};

    //=========FW upgrade========================*/

    pbt_buf = CTPM_FW;
    msleep(100);
    getVerNo(buf, BUFFER_LEN);
    a = buf[0];
    b = fts_ctpm_get_i_file_ver();
    printk("a == %hu,  b== %x \n",a, b);
    /*
     * when the firmware in touch panel maybe corrupted,
     * or the firmware in host flash is new, need upgrade
     */
    if ( 0xa6 == a || a == 0x11 ){
		printk("Jessica OK");
        /*call the upgrade function*/
        if(chip_id == 0x55 || chip_id == 0x08 || chip_id == 0x00 || chip_id == 0x0a || chip_id == 0x58){
            i_ret =  ft5x06_ctpm_fw_upgrade(client,&pbt_buf[0],sizeof(CTPM_FW));
            if (i_ret != 0){
                printk("[FTS] upgrade failed i_ret = %d.\n", i_ret);
            }
            else {
                printk("[FTS] upgrade successfully.\n");
#ifdef AUTO_CLB
                fts_ctpm_auto_clb();  //start auto CLB
#endif
            }
        }
    }
	printk("Jessica ERR");
    return i_ret;

}

unsigned char fts_ctpm_get_upg_ver(void)
{
    unsigned int ui_sz;
    ui_sz = sizeof(CTPM_FW);
    if (ui_sz > 2){
        return CTPM_FW[0];
    }
    else{
        return 0xff; //default value
    }
}

static int ft5x_i2c_rxdata(char *rxdata, int length)
{
    int ret;

    struct i2c_msg msgs[] = {
        {
            .addr   = this_client->addr,
            .flags  = 0,
            .len    = 1,
            .buf    = rxdata,
        },
        {
            .addr   = this_client->addr,
            .flags  = I2C_M_RD,
            .len    = length,
            .buf    = rxdata,
        },
    };
    ret = i2c_transfer(this_client->adapter, msgs, 2);
    if (ret < 0)
        printk("msg %s i2c read error: %d\n", __func__, ret);

    return ret;
}

static int ft5x_i2c_txdata(char *txdata, int length)
{
    int ret;

    struct i2c_msg msg[] = {
        {
            .addr   = this_client->addr,
            .flags  = 0,
            .len    = length,
            .buf    = txdata,
        },
    };

    //msleep(1);
    ret = i2c_transfer(this_client->adapter, msg, 1);
    if (ret < 0)
        pr_err("%s i2c write error: %d\n", __func__, ret);

    return ret;
}

static int ft5x_set_reg(u8 addr, u8 para)
{
    u8 buf[3];
    int ret = -1;

    buf[0] = addr;
    buf[1] = para;
    ret = ft5x_i2c_txdata(buf, 2);
    if (ret < 0) {
        pr_err("write reg failed! %#x ret: %d", buf[0], ret);
        return -1;
    }

    return 0;
}

/*Read touch point information when the interrupt  is asserted.*/
static int ft5x0x_read_Touchdata(struct ft5x_ts_data *data)
{
    struct ts_event *event = &data->event;
    u8 buf[POINT_READ_BUF] = { 0 };
    int ret = -1;
    int i = 0;
    u8 pointid = FT_MAX_ID;

    ret = ft5x_i2c_rxdata(buf, POINT_READ_BUF);
   if (ret < 0) {
        dev_err(&data->client->dev, "%s read touchdata failed.\n", __func__);
        return ret;
    }
    memset(event, 0, sizeof(struct ts_event));

    event->touch_point_num = buf[FT_TOUCH_POINT_NUM] & 0x0F;

    //dprintk(DEBUG_X_Y_INFO, "event->touch_point_num = %d\n", event->touch_point_num);

    event->touch_point = 0;
    for (i = 0; i < CFG_MAX_TOUCH_POINTS; i++) {

        pointid = (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;
        if (pointid >= FT_MAX_ID) {
            break;
        }

        event->touch_point++;
        event->au16_x[i] =
            (((s16) buf[FT_TOUCH_X_H_POS + FT_TOUCH_STEP * i]) & 0x0F) <<
            8 | (((s16) buf[FT_TOUCH_X_L_POS + FT_TOUCH_STEP * i])& 0xFF);
        event->au16_y[i] =
            (((s16) buf[FT_TOUCH_Y_H_POS + FT_TOUCH_STEP * i]) & 0x0F) <<
            8 | (((s16) buf[FT_TOUCH_Y_L_POS + FT_TOUCH_STEP * i]) & 0xFF);
        event->au8_touch_event[i] =
            buf[FT_TOUCH_EVENT_POS + FT_TOUCH_STEP * i] >> 6;
        event->au8_finger_id[i] =
            (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;
        event->au8_xy[i] = (unsigned char)buf[FT_TOUCH_XY_POS + FT_TOUCH_STEP * i];

        //dprintk(DEBUG_X_Y_INFO, "id=%d event=%d x=%d y=%d\n", event->au8_finger_id[i],
        //    event->au8_touch_event[i], event->au16_x[i], event->au16_y[i]);

    }

    event->pressure = FT_PRESS;

    return 0;
}

/*
*report the point information
*/
static void ft5x_report_value(struct ft5x_ts_data *data)
{
    struct ts_event *event = &data->event;
    int i;
    int uppoint = 0;
    int touchs = 0;
    int x,y;
    /*protocol B*/

    //dprintk(DEBUG_X_Y_INFO, "event->touch_point = %d\n", event->touch_point);

    for (i = 0; i < event->touch_point; i++)
    {
        input_mt_slot(data->input_dev, event->au8_finger_id[i]);

        if (event->au8_touch_event[i]== 0 || event->au8_touch_event[i] == 2) {
            input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);
            input_report_abs(data->input_dev, ABS_MT_PRESSURE, event->pressure);
            input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);

            x = event->au16_x[i];
            y = event->au16_y[i];
            if( data->exchange_x_y_flag ) {
                int t = x;
                x = y;
                y = t;
            }
            if( data->revert_x_flag) {
                x = data->screen_max_x - x;
            }
            if( data->revert_y_flag) {
                y = data->screen_max_y - y;
            }


            input_report_abs(data->input_dev, ABS_MT_POSITION_X, x);
            input_report_abs(data->input_dev, ABS_MT_POSITION_Y, y);

            touchs |= BIT(event->au8_finger_id[i]);
            event->touchs |= BIT(event->au8_finger_id[i]);

            //dprintk(DEBUG_X_Y_INFO, "au8_finger_id[%d] = %d:x = %d,y=%d,max_x=%d,may_y=%d\n", i ,
            //    event->au8_finger_id[i], x, y, data->screen_max_x, data->screen_max_y);

        } else {

            uppoint++;
            input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
            event->touchs &= ~BIT(event->au8_finger_id[i]);
        }
    }

    if(unlikely(event->touchs ^ touchs)){
        for(i = 0; i < CFG_MAX_TOUCH_POINTS; i++){
            // here 'i' is equal finger_id
            if(BIT(i) & (event->touchs ^ touchs)){
                //dprintk(DEBUG_X_Y_INFO, "release finger_id(%d).\n", i);
                input_mt_slot(data->input_dev, i);
                input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
            }
        }
    }
    event->touchs = touchs;

    if(event->touch_point == uppoint) {
        input_report_key(data->input_dev, BTN_TOUCH, 0);
    } else {
        input_report_key(data->input_dev, BTN_TOUCH, event->touch_point > 0);
    }

    input_sync(data->input_dev);

}

static void ft5x_ts_pen_irq_work(struct work_struct *work)
{
    struct ft5x_ts_data * ts = container_of(work, struct ft5x_ts_data, pen_event_work);
    if (ft5x0x_read_Touchdata(ts) == 0) {
        ft5x_report_value(ts);
    }

    ft5x_set_irq_enabled(ts, true);

    //dprintk(DEBUG_INT_INFO,"enter %s.\n", __func__);
}

irqreturn_t ft5x_ts_interrupt(int irq, void *dev_id)
{
    struct ft5x_ts_data *ts = (struct ft5x_ts_data *)dev_id;

    //dprintk(DEBUG_INT_INFO,"==========ft5x_ts TS Interrupt============\n");

    disable_irq_nosync(ts->irq);
    ts->irq_enabled = false;

    if (!work_pending(&ts->pen_event_work)) {
        queue_work(ts->ts_workqueue, &ts->pen_event_work);
    }

    return IRQ_HANDLED;
}

static void ft5x_resume_events(struct work_struct *work)
{
    int i = 0;
    struct ft5x_ts_data * data = container_of(work, struct ft5x_ts_data, resume_events_work);

    ft5x_set_power_enabled(data, true);
    delay_ms(10);
    ft5x_reset(data);

    if(chip_id == 0x02 ){
#ifdef FT5X02_CONFIG_INI
        if (ft5x02_Get_Param_From_Ini(FT5X02_CONFIG_NAME) >= 0)
            ft5x02_Init_IC_Param(this_client);
        else
            printk("Get ft5x02 param from INI file failed\n");
#else
        msleep(200);    /*wait...*/
        while(i<5){
            //dprintk(DEBUG_INIT,"-----------------------------------------Init ic param\r\n");
            if (ft5x02_Init_IC_Param(this_client) >=0 ){
                //dprintk(DEBUG_INIT,"---------------------------------------get ic param\r\n");
                if(ft5x02_get_ic_param(this_client) >=0)
                    break;
            }
            i++;
        }
#endif
        }

    ft5x_set_irq_enabled(data, true);

}

static int ft5x_ts_suspend(struct ft5x_ts_data *data)
{

    //dprintk(DEBUG_SUSPEND,"==ft5x_ts_suspend=\n");
    //dprintk(DEBUG_SUSPEND,"CONFIG_PM: write FT5X0X_REG_PMODE .\n");

    cancel_work_sync(&data->pen_event_work);
    flush_workqueue(data->ts_workqueue);

    ft5x_set_reg(FT5X0X_REG_PMODE, PMODE_HIBERNATE);
    delay_ms(10);

    ft5x_set_irq_enabled(data, false);
    ft5x_set_power_enabled(data, false);

    return 0;
}

static int ft5x_ts_resume(struct ft5x_ts_data *data)
{

    ///dprintk(DEBUG_SUSPEND,"==CONFIG_PM:ft5x_ts_resume== \n");

    queue_work(data->ts_workqueue, &data->resume_events_work);

    return 0;
}

static int ft5x_init_irq(struct ft5x_ts_data * data)
{
    int gpio;
    int ret = 0;
    if (!data->irq_gpio) {
        dev_err(&data->client->dev, "%s[%d], irq gpio is null.", __func__, __LINE__);
        return -ENOMEM;
    }
    gpio = desc_to_gpio(data->irq_gpio);
    if (gpio_is_valid(gpio)) {
        data->irq = gpio_to_irq(gpio);
    }

    ret = devm_request_irq(&data->client->dev, data->irq,
                ft5x_ts_interrupt,
                IRQ_GPIO_ACTIVE_VALUE ? IRQF_TRIGGER_RISING : IRQF_TRIGGER_FALLING,
                dev_name(&data->client->dev),
                data);
    if (ret) {
        dev_err(&data->client->dev, "%s[%d], Failed to request irq.\n", __func__, __LINE__);
    } else {
        dev_err(&data->client->dev, "%s[%d], ok to request irq, trigger is [%s].\n", __func__, __LINE__, IRQ_GPIO_ACTIVE_VALUE ? "rising" : "falling");
        data->irq_enabled = false;//true;
    }

    return ret;
}

static int ft5x_get_chip_id(struct i2c_client *client)
{
    int i = 0;
    int ret = 0;
	struct ft5x_ts_data *data = i2c_get_clientdata(client);//rocky
    if (!client) {
        return -ENOMEM;
    }

    while((ret == 0x00) || (ret == 0xa3)) {
        ret = i2c_smbus_read_byte_data(client, 0xA3);
        dev_warn(&client->dev, "%s[%d]: addr is 0x%x, chip_id value:0x%x (%d)\n", __func__, __LINE__, client->addr, ret, ret);
        if((i++) > 10) {
            break;
        }
        delay_ms(5);
	//add by rocky for After multiple reboots, touch has no effect ...
		//gpio_set_value(134, 0);//reset gpio
		gpiod_set_value(data->reset_gpio, RESET_GPIO_ACTIVE_VALUE);
		delay_ms(20);
		//gpio_set_value(134, 1);//reset gpio
		gpiod_set_value(data->reset_gpio, !RESET_GPIO_ACTIVE_VALUE);
		delay_ms(250);
	//add by rocky for After multiple reboots, touch has no effect ...
    }
    //dprintk(DEBUG_INIT, "read chip_id timers,timers=%d\n",i);

    return ret;

}

static void ft5x_init_events (struct work_struct *work)
{
    int i = 0;
    struct ft5x_ts_data * data = container_of(work, struct ft5x_ts_data, init_events_work);

    //dprintk(DEBUG_INIT, "====%s begin=====.  \n", __func__);
	
    /*gpiod_set_value(data->reset_gpio, RESET_GPIO_ACTIVE_VALUE);
    delay_ms(10);
    ft5x_set_power_enabled(data, true);
	delay_ms(10);
	gpiod_set_value(data->reset_gpio, !RESET_GPIO_ACTIVE_VALUE);
	delay_ms(200);

    ft5x_reset(data);*/ //delete for After multiple reboots, touch has no effect  by rocky

    chip_id = ft5x_get_chip_id(data->client);
	if(chip_id < 0){
	   	return -ENODEV;
	}


#ifdef CONFIG_SUPPORT_FTS_CTP_UPG
  fts_ctpm_fw_upgrade_with_i_file(data->client);
#endif

    if(chip_id == 0x02 ) {
#ifdef FT5X02_CONFIG_INI
        if (ft5x02_Get_Param_From_Ini(FT5X02_CONFIG_NAME) >= 0)
            ft5x02_Init_IC_Param(this_client);
        else
            printk("Get ft5x02 param from INI file failed\n");
#else
        msleep(1000);   /*wait...*/
        while(i<5){
            //dprintk(DEBUG_INIT,"-----------------------------------------Init ic param\r\n");
            if (ft5x02_Init_IC_Param(this_client) >=0 ) {
              //  dprintk(DEBUG_INIT,"---------------------------------------get ic param\r\n");
                if(ft5x02_get_ic_param(this_client) >=0)
                    break;
            }
            i++;
        }
#endif
    }

    //songsayit, here to init irq(irq enabled defautly)
    ft5x_init_irq(data);

}

#if defined(CONFIG_OF)
static int ft5x_parse_dt(struct ft5x_ts_data *ft5x_ts)
{
    struct device *dev = NULL;

    u32 val;
    int err = 0;

    if (!ft5x_ts) {
        printk("%s[%d]: ft5x_ts is null.\n", __func__, __LINE__);
        return -ENOMEM;
    }
    if (!ft5x_ts->client) {
        printk("%s[%d]: ft5x_ts->client is null.\n", __func__, __LINE__);
        return -ENOMEM;
    }
    dev = &ft5x_ts->client->dev;

    if (!dev->of_node) {
        dev_err(dev, "%s[%d]: no device node found.\n", __func__, __LINE__);
        return -ENOMEM;
    }

    ft5x_ts->supply = devm_regulator_get(dev, "power");
    if (IS_ERR(ft5x_ts->supply)) {
        dev_err(dev, "%s[%d]: failed to get supply of power.\n", __func__, __LINE__);
    }
    ft5x_ts->enable_gpio = devm_gpiod_get_optional(dev, "enable", ENABLE_GPIO_ACTIVE_VALUE ? GPIOD_OUT_LOW : GPIOD_OUT_HIGH);
    if (IS_ERR(ft5x_ts->enable_gpio)) {
        err = PTR_ERR(ft5x_ts->enable_gpio);
        dev_err(dev, "%s[%d]:failed to request enable GPIO: %d.\n", __func__, __LINE__, err);
    }
    ft5x_ts->reset_gpio = devm_gpiod_get_optional(dev, "reset", RESET_GPIO_ACTIVE_VALUE ? GPIOD_OUT_LOW : GPIOD_OUT_HIGH);
    if (IS_ERR(ft5x_ts->reset_gpio)) {
        err = PTR_ERR(ft5x_ts->reset_gpio);
        dev_err(dev, "%s[%d]:failed to request reset GPIO: %d.n", __func__, __LINE__, err);
    }
    ft5x_ts->irq_gpio = devm_gpiod_get_optional(dev, "irq", GPIOD_IN);
    if (IS_ERR(ft5x_ts->irq_gpio)) {
        err = PTR_ERR(ft5x_ts->irq_gpio);
        dev_err(dev, "%s[%d]:failed to request irq GPIO: %d.\n", __func__, __LINE__, err);
    }

    if (!of_property_read_u32(dev->of_node, "screen_max_x", &val)) {
        ft5x_ts->screen_max_x = val;
    } else {
        ft5x_ts->screen_max_x = DEFAULT_SCREEN_MAX_X;
    }

    if (!of_property_read_u32(dev->of_node, "screen_max_y", &val)) {
        ft5x_ts->screen_max_y = val;
    } else {
        ft5x_ts->screen_max_y = DEFAULT_SCREEN_MAX_Y;
    }

    ft5x_ts->revert_x_flag = 0;
    if (!of_property_read_u32(dev->of_node, "revert_x_flag", &val))
        ft5x_ts->revert_x_flag = !!val;
    ft5x_ts->revert_y_flag = 0;
    if (!of_property_read_u32(dev->of_node, "revert_y_flag", &val))
        ft5x_ts->revert_y_flag = !!val;
    ft5x_ts->exchange_x_y_flag = 0;
    if (!of_property_read_u32(dev->of_node, "exchange_x_y_flag", &val))
        ft5x_ts->exchange_x_y_flag = !!val;


    //dprintk(DEBUG_INIT, "screen_max_x = %d, screen_max_y = %d.\n", ft5x_ts->screen_max_x, ft5x_ts->screen_max_y);

    return 0;
}
#else
static int ft5x_parse_dt(struct ft5x_ts_data *ft5x_ts)
{
    return 0;
}

#endif

static int ft5x_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct ft5x_ts_data *ft5x_ts;
    struct input_dev *input_dev;
    int err = 0;

#ifdef TOUCH_KEY_SUPPORT
    int i = 0;
#endif

    //dprintk(DEBUG_INIT, "====%s begin=====.  \n", __func__);

    //printk( "====%s begin===songshtian==.  \n", __func__);

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        err = -ENODEV;
        dev_err(&client->dev, "i2c_check_functionality failed\n");
        return err;
    }

    ft5x_ts = devm_kzalloc(&client->dev, sizeof(*ft5x_ts), GFP_KERNEL);
    if (!ft5x_ts)   {
        err = -ENOMEM;
        dev_err(&client->dev, "alloc data failed\n");
        return err;
    }
	

    this_client = client;
    ft5x_ts->client = client;
    i2c_set_clientdata(client, ft5x_ts);
    ft5x_parse_dt(ft5x_ts);
	
    ft5x_ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
    if (!ft5x_ts->ts_workqueue) {
        err = -ENOMEM;
        dev_err(&client->dev, "failed to create ts_workqueue.\n");
        goto failed_create_singlethread_workqueue;
    }

    mutex_init(&ft5x_ts->mutex_lock);

    INIT_WORK(&ft5x_ts->pen_event_work, ft5x_ts_pen_irq_work);

    INIT_WORK(&ft5x_ts->init_events_work, ft5x_init_events);
    INIT_WORK(&ft5x_ts->resume_events_work, ft5x_resume_events);


    input_dev = input_allocate_device();
    if (!input_dev) {
        err = -ENOMEM;
        dev_err(&client->dev, "failed to allocate input device\n");
        goto exit_input_dev_alloc_failed;
    }

    ft5x_ts->input_dev = input_dev;
    input_dev->name = dev_name(&client->dev);
    input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_set_drvdata(input_dev, ft5x_ts);


    __set_bit(EV_ABS, input_dev->evbit);
    __set_bit(EV_KEY, input_dev->evbit);
    __set_bit(EV_REP, input_dev->evbit);
    __set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
    input_mt_init_slots(input_dev, TOUCH_POINT_NUM, INPUT_MT_DIRECT);

#ifdef CONFIG_FT5X0X_MULTITOUCH
    set_bit(ABS_MT_POSITION_X, input_dev->absbit);
    set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
    set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
    set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);
    input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, ft5x_ts->screen_max_x, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, ft5x_ts->screen_max_y, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);

    #ifdef FOR_TSLIB_TEST
    set_bit(BTN_TOUCH, input_dev->keybit);
    #endif

    #ifdef TOUCH_KEY_SUPPORT
    key_tp = 0;
    input_dev->evbit[0] = BIT_MASK(EV_KEY);
    for (i = 1; i < TOUCH_KEY_NUMBER; i++)
        set_bit(i, input_dev->keybit);
    #endif
#else
    set_bit(ABS_X, input_dev->absbit);
    set_bit(ABS_Y, input_dev->absbit);
    set_bit(ABS_PRESSURE, input_dev->absbit);
    set_bit(BTN_TOUCH, input_dev->keybit);
    input_set_abs_params(input_dev, ABS_X, 0, ft5x_ts->screen_max_x, 0, 0);
    input_set_abs_params(input_dev, ABS_Y, 0, ft5x_ts->screen_max_y, 0, 0);
    input_set_abs_params(input_dev, ABS_PRESSURE, 0, PRESS_MAX, 0 , 0);
#endif

    err = input_register_device(input_dev);
    if (err) {
        dev_err(&client->dev,"ft5x_ts_probe: failed to register input device: %s\n",
                dev_name(&client->dev));
        goto exit_input_register_device_failed;
    }

    queue_work(ft5x_ts->ts_workqueue, &ft5x_ts->init_events_work);

#ifdef CONFIG_FT5X0X_MULTITOUCH
    //dprintk(DEBUG_INIT,"CONFIG_FT5X0X_MULTITOUCH is defined. \n");
#endif
    //dprintk(DEBUG_INIT, "==%s over =\n", __func__);

    return 0;

exit_input_register_device_failed:
    input_free_device(input_dev);
exit_input_dev_alloc_failed:
    i2c_set_clientdata(client, NULL);
    if (ft5x_ts) {
        destroy_workqueue(ft5x_ts->ts_workqueue);
    }
failed_create_singlethread_workqueue:
    if (ft5x_ts) {
        kfree(ft5x_ts);
    }
    return err;
}

static int ft5x_ts_remove(struct i2c_client *client)
{

    struct ft5x_ts_data *data = i2c_get_clientdata(client);
    ft5x_set_reg(FT5X0X_REG_PMODE, PMODE_HIBERNATE);

    printk("==ft5x_ts_remove=\n");

    cancel_work_sync(&data->init_events_work);
    cancel_work_sync(&data->resume_events_work);
    cancel_work_sync(&data->pen_event_work);
    destroy_workqueue(data->ts_workqueue);

    mutex_destroy(&data->mutex_lock);

    input_unregister_device(data->input_dev);
    input_free_device(data->input_dev);

    ft5x_set_power_enabled(data, false);
    ft5x_set_irq_enabled(data, false);

    if (data->supply) {
        devm_regulator_put(data->supply);
    }
    if (data->enable_gpio) {
        devm_gpiod_put(&client->dev, data->enable_gpio);
    }
    if (data->reset_gpio) {
        devm_gpiod_put(&client->dev, data->reset_gpio);
    }

    if (data->irq_gpio) {
        if (data->irq > 0) {
            free_irq(data->irq, data);
        }
        devm_gpiod_put(&client->dev, data->irq_gpio);
    }

    kfree(data);

    i2c_set_clientdata(client, NULL);

    return 0;

}


#ifdef CONFIG_PM
static int ft5x_suspend(struct device *dev)
{
    struct ft5x_ts_data *data = dev_get_drvdata(dev);

    ft5x_ts_suspend(data);

    return 0;
}

static int ft5x_resume(struct device *dev)
{
    struct ft5x_ts_data *data = dev_get_drvdata(dev);

    ft5x_ts_resume(data);

    return 0;
}

static const struct dev_pm_ops ft5x_pm_ops = {
    .suspend	= ft5x_suspend,
    .resume		= ft5x_resume,
};
#endif


static const struct i2c_device_id ft5x_ts_id[] = {
    { CTP_NAME, 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, ft5x_ts_id);



#if defined(CONFIG_OF)
static struct of_device_id ft5x_dt_ids[] = {
	{ .compatible = CTP_NAME },
};

MODULE_DEVICE_TABLE(of, ft5x_dt_ids);
#endif


static struct i2c_driver ft5x_ts_driver = {
    .driver = {
        .name = CTP_NAME,
        .owner = THIS_MODULE,
#if defined(CONFIG_OF)
        .of_match_table = of_match_ptr(ft5x_dt_ids),
#endif
#ifdef CONFIG_PM
        .pm = &ft5x_pm_ops,
#endif
    },
    .probe      = ft5x_ts_probe,
    .remove     = ft5x_ts_remove,
    .id_table   = ft5x_ts_id,

};
module_i2c_driver(ft5x_ts_driver);

MODULE_AUTHOR("<wenfs@Focaltech-systems.com>");
MODULE_DESCRIPTION("FocalTech ft5x TouchScreen driver");
MODULE_LICENSE("GPL");

