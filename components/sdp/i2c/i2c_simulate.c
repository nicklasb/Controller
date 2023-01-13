#include "i2c_simulate.h"
#include <sdkconfig.h>
#ifdef CONFIG_SDP_SIM

int bad_crc_count = 0;

unsigned int sim_bad_crc(unsigned int crc) {
    if (bad_crc_count < CONFIG_SDP_SIM_I2C_BAD_CRC) {
        bad_crc_count ++;
        return crc + 1;
    }
    
    return crc;
} 

#endif