#ifndef _I2C_H_
#define _I2C_H_

#include <sdkconfig.h>
#ifdef CONFIG_SDP_LOAD_I2C

void i2c_init(char * _log_prefix);

void i2c_shutdown();

#endif
#endif