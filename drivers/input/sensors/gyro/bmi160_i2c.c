/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmi160_i2c.c
 * @date     2014/11/25 14:40
 * @id       "20f77db"
 * @version  1.3
 *
 * @brief
 * This file implements moudle function, which add
 * the driver to I2C core.
*/

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "bmi160_driver.h"
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/sensor-dev.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>

#define OUT_X_L_G	0x0c
#define WHO_AM_I_G	0x00
#define bmi160_DEVICE_ID_G	0xD1
#define CTRL_REG1_G	0x7e
#define INT1_SRC_G	0x00

#define BMG_AXIS_X				0
#define BMG_AXIS_Y				1
#define BMG_AXIS_Z				2
#define BMG_AXES_NUM			3
#define BMG_DATA_LEN			6
#define BMG_BUFSIZE				128
#define C_MAX_FIR_LENGTH		(32)
#define MAX_SENSOR_NAME			(32)

/* sensor type */
enum SENSOR_TYPE_ENUM { BMI160_GYRO_TYPE = 0x0, INVALID_TYPE = 0xff };

/* range */
enum BMG_RANGE_ENUM {
	BMG_RANGE_2000 = 0x0, /* +/- 2000 degree/s */
	BMG_RANGE_1000,       /* +/- 1000 degree/s */
	BMG_RANGE_500,	/* +/- 500 degree/s */
	BMG_RANGE_250,	/* +/- 250 degree/s */
	BMG_RANGE_125,	/* +/- 125 degree/s */
	BMG_UNDEFINED_RANGE = 0xff
};

/* power mode */
enum BMG_POWERMODE_ENUM {
	BMG_SUSPEND_MODE = 0x0,
	BMG_NORMAL_MODE,
	BMG_UNDEFINED_POWERMODE = 0xff
};

/* debug information flags */
enum GYRO_TRC {
	GYRO_TRC_FILTER = 0x01,
	GYRO_TRC_RAWDATA = 0x02,
	GYRO_TRC_IOCTL = 0x04,
	GYRO_TRC_CALI = 0x08,
	GYRO_TRC_INFO = 0x10,
};

/* s/w data filter */
struct data_filter {
	s16 raw[C_MAX_FIR_LENGTH][BMG_AXES_NUM];
	int sum[BMG_AXES_NUM];
	int num;
	int idx;
};

/*! @defgroup bmi160_i2c_src
 *  @brief bmi160 i2c driver module
 @{*/

static struct i2c_client *bmi_client;
#if 0
/*!
 * @brief define i2c wirte function
 *
 * @param client the pointer of i2c client
 * @param reg_addr register address
 * @param data the pointer of data buffer
 * @param len block size need to write
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
/*	i2c read routine for API*/
static s8 bmi_i2c_read(struct i2c_client *client, u8 reg_addr,
			u8 *data, u8 len)
	{
#if !defined BMI_USE_BASIC_I2C_FUNC
		s32 dummy;
		if (NULL == client)
			return -EINVAL;

		while (0 != len--) {
#ifdef BMI_SMBUS
			dummy = i2c_smbus_read_byte_data(client, reg_addr);
			if (dummy < 0) {
				dev_err(&client->dev, "i2c smbus read error");
				return -EIO;
			}
			*data = (u8)(dummy & 0xff);
#else
			dummy = i2c_master_send(client, (char *)&reg_addr, 1);
			if (dummy < 0) {
				dev_err(&client->dev, "i2c bus master write error");
				return -EIO;
			}

			dummy = i2c_master_recv(client, (char *)data, 1);
			if (dummy < 0) {
				dev_err(&client->dev, "i2c bus master read error");
				return -EIO;
			}
#endif
			reg_addr++;
			data++;
		}
		return 0;
#else
		int retry;

		struct i2c_msg msg[] = {
			{
			 .addr = client->addr,
			 .flags = 0,
			 .len = 1,
			 .buf = &reg_addr,
			},

			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = len,
			 .buf = data,
			 },
		};

		for (retry = 0; retry < BMI_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg,
						ARRAY_SIZE(msg)) > 0)
				break;
			else
				usleep_range(BMI_I2C_WRITE_DELAY_TIME * 1000,
				BMI_I2C_WRITE_DELAY_TIME * 1000);
		}

		if (BMI_MAX_RETRY_I2C_XFER <= retry) {
			dev_err(&client->dev, "I2C xfer error");
			return -EIO;
		}

		return 0;
#endif
	}

#endif
static s8 bmi_i2c_burst_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u16 len)
{
	int retry;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg_addr,
		},

		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

	for (retry = 0; retry < BMI_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			usleep_range(BMI_I2C_WRITE_DELAY_TIME * 1000,
				BMI_I2C_WRITE_DELAY_TIME * 1000);
	}

	if (BMI_MAX_RETRY_I2C_XFER <= retry) {
		dev_err(&client->dev, "I2C xfer error");
		return -EIO;
	}

	return 0;
}

#if 0
/* i2c write routine for */
static s8 bmi_i2c_write(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len)
{
#if !defined BMI_USE_BASIC_I2C_FUNC
	s32 dummy;

#ifndef BMI_SMBUS
	u8 buffer[2];
#endif

	if (NULL == client)
		return -EPERM;

	while (0 != len--) {
#ifdef BMI_SMBUS
		dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
#else
		buffer[0] = reg_addr;
		buffer[1] = *data;
		dummy = i2c_master_send(client, (char *)buffer, 2);
#endif
		reg_addr++;
		data++;
		if (dummy < 0) {
			dev_err(&client->dev, "error writing i2c bus");
			return -EPERM;
		}

	}
	usleep_range(BMI_I2C_WRITE_DELAY_TIME * 1000,
	BMI_I2C_WRITE_DELAY_TIME * 1000);
	return 0;
#else
	u8 buffer[2];
	int retry;
	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 2,
		 .buf = buffer,
		 },
	};

	while (0 != len--) {
		buffer[0] = reg_addr;
		buffer[1] = *data;
		for (retry = 0; retry < BMI_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg,
						ARRAY_SIZE(msg)) > 0) {
				break;
			} else {
				usleep_range(BMI_I2C_WRITE_DELAY_TIME * 1000,
				BMI_I2C_WRITE_DELAY_TIME * 1000);
			}
		}
		if (BMI_MAX_RETRY_I2C_XFER <= retry) {
			dev_err(&client->dev, "I2C xfer error");
			return -EIO;
		}
		reg_addr++;
		data++;
	}

	usleep_range(BMI_I2C_WRITE_DELAY_TIME * 1000,
	BMI_I2C_WRITE_DELAY_TIME * 1000);
	return 0;
#endif
}
#endif
#if 0
static s8 bmi_i2c_read_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	int err = 0;
	err = bmi_i2c_read(bmi_client, reg_addr, data, len);
	return err;
}

static s8 bmi_i2c_write_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	int err = 0;
	err = bmi_i2c_write(bmi_client, reg_addr, data, len);
	return err;
}
#endif
s8 bmi_burst_read_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u16 len)
{
	int err = 0;
	err = bmi_i2c_burst_read(bmi_client, reg_addr, data, len);
	return err;
}
EXPORT_SYMBOL(bmi_burst_read_wrapper);
/*!
 * @brief BMI probe function via i2c bus
 *
 * @param client the pointer of i2c client
 * @param id the pointer of i2c device id
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
#if 0
static int bmi_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
		int err = 0;
		struct bmi_client_data *client_data = NULL;

		dev_info(&client->dev, "BMI160 i2c function probe entrance");

		if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
			dev_err(&client->dev, "i2c_check_functionality error!");
			err = -EIO;
			goto exit_err_clean;
		}

		if (NULL == bmi_client) {
			bmi_client = client;
		} else {
			dev_err(&client->dev,
				"this driver does not support multiple clients");
			err = -EBUSY;
			goto exit_err_clean;
		}

		client_data = kzalloc(sizeof(struct bmi_client_data),
							GFP_KERNEL);
		if (NULL == client_data) {
			dev_err(&client->dev, "no memory available");
			err = -ENOMEM;
			goto exit_err_clean;
		}

		client_data->device.bus_read = bmi_i2c_read_wrapper;
		client_data->device.bus_write = bmi_i2c_write_wrapper;
		dev_info(&client->dev, "BMI160 i2c function probe ok"); 
		return err;//bmi_probe(client_data, &client->dev);

exit_err_clean:
dev_info(&client->dev, "BMI160 i2c function probe err"); 
		if (err)
			bmi_client = NULL;
		return err;
}

static int bmi_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int err = 0;
	err = bmi_suspend(&client->dev);
	return err;
}

static int bmi_i2c_resume(struct i2c_client *client)
{
	int err = 0;

	/* post resume operation */
	err = bmi_resume(&client->dev);

	return err;
}


static int bmi_i2c_remove(struct i2c_client *client)
{
	int err = 0;
	//err = bmi_remove(&client->dev);
	bmi_client = NULL;

	return err;
}



static const struct i2c_device_id bmi_id[] = {
	{SENSOR_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, bmi_id);

static const struct of_device_id bmi160_of_match[] = {
	{ .compatible = "bosch-sensortec,bmi160", },
	{ .compatible = "bmi160", },
	{ .compatible = "bosch, bmi160", },
	{ }
};
MODULE_DEVICE_TABLE(of, bmi160_of_match);

static struct i2c_driver bmi_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
		.of_match_table = bmi160_of_match,
	},
	.class = I2C_CLASS_HWMON,
	.id_table = bmi_id,
	.probe = bmi_i2c_probe,
	.remove = bmi_i2c_remove,
//	.suspend = bmi_i2c_suspend,
//	.resume = bmi_i2c_resume,
};

static int __init BMI_i2c_init(void)
{
	return i2c_add_driver(&bmi_i2c_driver);
}

static void __exit BMI_i2c_exit(void)
{
	i2c_del_driver(&bmi_i2c_driver);
}

MODULE_AUTHOR("Contact <contact@bosch-sensortec.com>");
MODULE_DESCRIPTION("driver for " SENSOR_NAME);
MODULE_LICENSE("GPL v2");

module_init(BMI_i2c_init);
module_exit(BMI_i2c_exit);
#endif 


static int sensor_active(struct i2c_client *client, int enable, int rate)
{
	struct sensor_private_data *sensor =
	    (struct sensor_private_data *)i2c_get_clientdata(client);
	int result = 0;

	//sensor->ops->ctrl_data = sensor_read_reg(client, sensor->ops->ctrl_reg);
	if (enable)
		result = sensor_write_reg(client,sensor->ops->ctrl_reg,0x17);
	else
		result = sensor_write_reg(client,sensor->ops->ctrl_reg,0x15);

	if (result)
		dev_err(&client->dev, "%s:fail to active sensor\n", __func__);
	sensor->ops->ctrl_data = sensor_read_reg(client, sensor->ops->ctrl_reg);
	DBG("%s:reg=0x%x,reg_ctrl=0x%x,enable=%d\n",
		__func__,
		sensor->ops->ctrl_reg,
		sensor->ops->ctrl_data, enable);

	return result;
}

static int sensor_init(struct i2c_client *client)
{
	//struct sensor_private_data *sensor =
	//    (struct sensor_private_data *)i2c_get_clientdata(client);
	int result = 0;
	//int reg = 0x00;
	//int reg1 = 0x01;

	int ret = sensor_write_reg(client, 0x7e, 0x15);// set normal
	    ret = sensor_write_reg(client, 0x41, 0x03);// set 2G
	    ret = sensor_write_reg(client, 0x42, 0x0D);// set gyr_odr 3200
	    ret = sensor_write_reg(client, 0x43, 0x00);// set gyr_range 2000
	printk("bmi160 sensor ret = %d init 0x00 = %x, 0x02 = %x 0x03 = %x 0x43 = %x \n",ret,sensor_read_reg(client, 0x00),sensor_read_reg(client, 0x02),sensor_read_reg(client, 0x03),sensor_read_reg(client, 0x43));
	return result;
}

static int gyro_report_value(struct i2c_client *client,
				struct sensor_axis *axis)
{
	struct sensor_private_data *sensor =
		(struct sensor_private_data *)i2c_get_clientdata(client);

	if (sensor->status_cur == SENSOR_ON) {
		input_report_rel(sensor->input_dev, ABS_RX, axis->x);
		input_report_rel(sensor->input_dev, ABS_RY, axis->y);
		input_report_rel(sensor->input_dev, ABS_RZ, axis->z);
		input_sync(sensor->input_dev);
	}

	return 0;
}

static int sensor_report_value(struct i2c_client *client)
{
	struct sensor_private_data *sensor =
		(struct sensor_private_data *) i2c_get_clientdata(client);
	struct sensor_platform_data *pdata = sensor->pdata;
	int ret = 0;
	short x, y, z;
	struct sensor_axis axis;
	u8 buffer[6] = {0};
	char value = 0;

	if (sensor->ops->read_len < 6) {
		dev_err(&client->dev, "%s:lenth is error,len=%d\n", __func__, sensor->ops->read_len);
		return -1;
	}

	memset(buffer, 0, 6);

	do {
		*buffer = sensor->ops->read_reg;
		ret = sensor_rx_data(client, buffer, sensor->ops->read_len);
		if (ret < 0)
			return ret;
	} while (0);

	x = ((buffer[1] << 8) & 0xFF00) + (buffer[0] & 0xFF);
	y = ((buffer[3] << 8) & 0xFF00) + (buffer[2] & 0xFF);
	z = ((buffer[5] << 8) & 0xFF00) + (buffer[4] & 0xFF);
	//printk("%d,%d,%d bmi160\n",x,y,z);
	axis.x = (pdata->orientation[0]) * x + (pdata->orientation[1]) * y + (pdata->orientation[2]) * z;
	axis.y = (pdata->orientation[3]) * x + (pdata->orientation[4]) * y + (pdata->orientation[5]) * z;
	axis.z = (pdata->orientation[6]) * x + (pdata->orientation[7]) * y + (pdata->orientation[8]) * z;

	gyro_report_value(client, &axis);

	mutex_lock(&(sensor->data_mutex));
	sensor->axis = axis;
	mutex_unlock(&(sensor->data_mutex));

	if ((sensor->pdata->irq_enable) && (sensor->ops->int_status_reg >= 0))
		value = sensor_read_reg(client, sensor->ops->int_status_reg);

	return ret;
}

struct sensor_operate gyro_bmi160_ops = {
	.name			= "bmi160_gyro",
	.type			= SENSOR_TYPE_GYROSCOPE,
	.id_i2c			= GYRO_ID_BMI160,
	.read_reg		= OUT_X_L_G,
	.read_len		= 6,
	.id_reg			= WHO_AM_I_G,
	.id_data		= bmi160_DEVICE_ID_G,
	.precision		= 16,
	.ctrl_reg		= CTRL_REG1_G,
	.int_status_reg	= INT1_SRC_G,
	.range			= {-32768, 32768},
	.trig			= IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
	.active			= sensor_active,
	.init			= sensor_init,
	.report			= sensor_report_value,
};
#if 0
static struct sensor_operate *gyro_get_ops(void)
{
	return &gyro_bmi160_ops;
}

static int __init gyro_bmi160_init(void)
{
	struct sensor_operate *ops = gyro_get_ops();
	int result = 0;
	int type = ops->type;
	printk("bmi160 init\n");
	result = sensor_register_slave(type, NULL, NULL, gyro_get_ops);

	return result;
}

static void __exit gyro_bmi160_exit(void)
{
	struct sensor_operate *ops = gyro_get_ops();
	int type = ops->type;

	sensor_unregister_slave(type, NULL, NULL, gyro_get_ops);
}

module_init(gyro_bmi160_init);
module_exit(gyro_bmi160_exit);
#endif

//modify by rocky #################
static int gyro_bmi160_probe(struct i2c_client *client,
			     const struct i2c_device_id *devid)
{
	return sensor_register_device(client, NULL, devid, &gyro_bmi160_ops);
}

static int gyro_bmi160_remove(struct i2c_client *client)
{
	return sensor_unregister_device(client, NULL, &gyro_bmi160_ops);
}

static const struct i2c_device_id gyro_bmi160_id[] = {
	{"bmi160_gyro", GYRO_ID_BMI160},
	{}
};

static struct i2c_driver gyro_bmi160_driver = {
	.probe = gyro_bmi160_probe,
	.remove = gyro_bmi160_remove,
	.shutdown = sensor_shutdown,
	.id_table = gyro_bmi160_id,
	.driver = {
		.name = "gyro_bmi160",
	#ifdef CONFIG_PM
		.pm = &sensor_pm_ops,
	#endif
	},
};

module_i2c_driver(gyro_bmi160_driver);

MODULE_AUTHOR("Rocky");
MODULE_DESCRIPTION("bmi160 3-Axis Gyroscope driver");
MODULE_LICENSE("GPL");