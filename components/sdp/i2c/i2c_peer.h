#define _I2C_PEER_H_
#ifdef _I2C_PEER_H_

#include "sdp_peer.h"

int i2c_unknown_counter;
int i2c_unknown_failures;
int i2c_crc_failures;


float i2c_score_peer(sdp_peer *peer, int data_length);
void i2c_stat_reset(sdp_peer *peer);
void i2c_peer_init_peer(sdp_peer *peer);

void i2c_peer_init(char * _log_prefix);

#endif