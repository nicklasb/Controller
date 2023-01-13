/**
 * @file i2c_simulate.h
 * @author Nicklas Borjesson 
 * @brief The worker reacts to items appearing in the worker queue 
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <sdkconfig.h>
#ifdef CONFIG_SDP_SIM

#ifndef _I2C_SIMULATE_H_
#define _I2C_SIMULATE_H_




unsigned int sim_bad_crc(unsigned int crc);


#endif
#endif