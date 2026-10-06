#ifndef SENSOR_I2C_MUTEX_H
#define SENSOR_I2C_MUTEX_H

int sensor_i2c_mutex_open(void);
int sensor_i2c_mutex_lock(int fd);
int sensor_i2c_mutex_unlock(int fd);
int sensor_i2c_mutex_close(int fd);

#endif
