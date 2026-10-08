#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/iio/iio.h>
#include <linux/module.h>

#define AHT20_CMD_INIT       0xBE
#define AHT20_CMD_TRIGGER    0xAC

#define AHT20_STATUS_BUSY    BIT(7)
#define AHT20_STATUS_CALIB   BIT(3)

#define AHT20_MEASUREMENT_TIMEOUT_MS 100

struct aht20_data {
	struct i2c_client *client;
};

static u8 aht20_crc8(const u8 *data, size_t len)
{
	u8 crc = 0xFF;
	int i;
	int bit;

	for (i = 0; i < len; i++) {
		crc ^= data[i];

		for (bit = 0; bit < 8; bit++) {
			if (crc & 0x80)
				crc = (crc << 1) ^ 0x31;
			else
				crc <<= 1;
		}
	}

	return crc;
}

static int aht20_read_status(struct aht20_data *data, u8 *status)
{
	struct i2c_client *client = data->client;
	int ret;

	ret = i2c_master_recv(client, status, 1);
	if (ret < 0) {
		dev_err(&client->dev,
			"Failed to read status: %d\n",
			ret);
		return ret;
	}

	if (ret != 1) {
		dev_err(&client->dev,
			"Short status read: %d bytes\n",
			ret);
		return -EIO;
	}

	return 0;
}

static int aht20_initialize(struct aht20_data *data)
{
	struct i2c_client *client = data->client;
	u8 status;
	u8 cmd[3] = {
		AHT20_CMD_INIT,
		0x08,
		0x00
	};
	int ret;

	ret = aht20_read_status(data, &status);
	if (ret)
		return ret;

	dev_info(&client->dev,
		 "AHT20 status: 0x%02x\n",
		 status);

	if (status & AHT20_STATUS_CALIB) {
		dev_info(&client->dev,
			 "AHT20 already calibrated\n");
		return 0;
	}

	dev_info(&client->dev,
		 "AHT20 not calibrated, initialising\n");

	ret = i2c_master_send(client, cmd, sizeof(cmd));
	if (ret < 0) {
		dev_err(&client->dev,
			"Failed to send initialisation command: %d\n",
			ret);
		return ret;
	}

	if (ret != sizeof(cmd)) {
		dev_err(&client->dev,
			"Short initialisation command: %d bytes\n",
			ret);
		return -EIO;
	}

	msleep(10);

	ret = aht20_read_status(data, &status);
	if (ret)
		return ret;

	if (!(status & AHT20_STATUS_CALIB)) {
		dev_err(&client->dev,
			"AHT20 calibration did not complete, status=0x%02x\n",
			status);
		return -EIO;
	}

	dev_info(&client->dev,
		 "AHT20 initialisation complete\n");

	return 0;
}

static int aht20_measure(struct aht20_data *data,
			 s32 *temperature,
			 s32 *humidity)
{
	struct i2c_client *client = data->client;
	u8 cmd[3] = {
		AHT20_CMD_TRIGGER,
		0x33,
		0x00
	};
	u8 buf[7];
	u8 status = 0;
	u8 crc;
	u32 raw_humidity;
	u32 raw_temperature;
	u32 temperature_hundredths;
	u32 humidity_hundredths;
	int ret;
	int elapsed_ms;

	ret = i2c_master_send(client, cmd, sizeof(cmd));
	if (ret < 0) {
		dev_err(&client->dev,
			"Failed to send measurement command: %d\n",
			ret);
		return ret;
	}

	if (ret != sizeof(cmd)) {
		dev_err(&client->dev,
			"Short measurement command: %d bytes\n",
			ret);
		return -EIO;
	}

	/*
	 * Poll the AHT20 BUSY bit.
	 *
	 * The status byte is returned directly by an I2C read;
	 * there is no command byte preceding the read.
	 */
	for (elapsed_ms = 0;
	     elapsed_ms < AHT20_MEASUREMENT_TIMEOUT_MS;
	     elapsed_ms += 5) {

		msleep(5);

		ret = aht20_read_status(data, &status);
		if (ret)
			return ret;

		if (!(status & AHT20_STATUS_BUSY))
			break;
	}

	if (status & AHT20_STATUS_BUSY) {
		dev_err(&client->dev,
			"AHT20 measurement timed out after %d ms\n",
			AHT20_MEASUREMENT_TIMEOUT_MS);
		return -ETIMEDOUT;
	}

	ret = i2c_master_recv(client, buf, sizeof(buf));
	if (ret < 0) {
		dev_err(&client->dev,
			"Failed to read measurement: %d\n",
			ret);
		return ret;
	}

	if (ret != sizeof(buf)) {
		dev_err(&client->dev,
			"Short measurement read: %d bytes\n",
			ret);
		return -EIO;
	}

	if (buf[0] & AHT20_STATUS_BUSY) {
		dev_err(&client->dev,
			"AHT20 is still busy\n");
		return -EBUSY;
	}

	crc = aht20_crc8(buf, 6);

	if (crc != buf[6]) {
		dev_err(&client->dev,
			"AHT20 CRC error: calculated=0x%02x received=0x%02x\n",
			crc, buf[6]);
		return -EIO;
	}

	raw_humidity =
		((u32)buf[1] << 12) |
		((u32)buf[2] << 4) |
		((u32)buf[3] >> 4);

	raw_temperature =
		((u32)(buf[3] & 0x0f) << 16) |
		((u32)buf[4] << 8) |
		(u32)buf[5];

	temperature_hundredths =
		(raw_temperature * 1250) >> 16;

	*temperature =
		(s32)(temperature_hundredths * 10) - 50000;

	humidity_hundredths =
		(raw_humidity * 625) >> 16;

	*humidity =
		(s32)(humidity_hundredths * 10);

	return 0;
}

static int aht20_read_raw(struct iio_dev *indio_dev,
			  struct iio_chan_spec const *chan,
			  int *val,
			  int *val2,
			  long mask)
{
	struct aht20_data *data = iio_priv(indio_dev);
	s32 temperature;
	s32 humidity;
	int ret;

	if (mask != IIO_CHAN_INFO_PROCESSED)
		return -EINVAL;

	ret = aht20_measure(data, &temperature, &humidity);
	if (ret)
		return ret;

	if (chan->type == IIO_TEMP) {
		*val = temperature;
		return IIO_VAL_INT;
	}

	if (chan->type == IIO_HUMIDITYRELATIVE) {
		*val = humidity;
		return IIO_VAL_INT;
	}

	return -EINVAL;
}

static const struct iio_info aht20_iio_info = {
	.read_raw = aht20_read_raw,
};

static const struct iio_chan_spec aht20_channels[] = {
	{
		.type = IIO_TEMP,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
	},
	{
		.type = IIO_HUMIDITYRELATIVE,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
	},
};

static int aht20_probe(struct i2c_client *client)
{
	struct iio_dev *indio_dev;
	struct aht20_data *data;
	int ret;

	indio_dev = devm_iio_device_alloc(&client->dev,
					  sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	data->client = client;

	indio_dev->name = "aht20";
	indio_dev->info = &aht20_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = aht20_channels;
	indio_dev->num_channels = ARRAY_SIZE(aht20_channels);

	i2c_set_clientdata(client, indio_dev);

	dev_info(&client->dev,
		 "AHT20 IIO device found at address 0x%02x\n",
		 client->addr);

	ret = aht20_initialize(data);
	if (ret) {
		dev_err(&client->dev,
			"AHT20 initialisation failed: %d\n",
			ret);
		return ret;
	}

	return devm_iio_device_register(&client->dev, indio_dev);
}

static const struct of_device_id aht20_of_match[] = {
	{
		.compatible = "aosong,aht20",
	},
	{ }
};

MODULE_DEVICE_TABLE(of, aht20_of_match);

static struct i2c_driver aht20_driver = {
	.driver = {
		.name = "aht20",
		.of_match_table = aht20_of_match,
	},
	.probe = aht20_probe,
};

module_i2c_driver(aht20_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Steve");
MODULE_DESCRIPTION("AHT20 I2C IIO driver");
