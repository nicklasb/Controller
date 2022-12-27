/**
 * @file qos.c
 * @author Nicklas Börjesson (nicklasb@gmail.com)
 * @brief Quality of Service functionality 
 * 
 * @version 0.1
 * @date 2022-12-23
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "qos.h"

/**
* @brief 
* TODO: 
* Re-send functionality. Or should this be a responsible of the sender? Maybe inside a connection?
* Or is retries and trying with other media on different levels?
* Actually; don't one have to retry to conclude that something perhaps need to change?
* Quality of media analysis.
*
* Modes:
* Security(OOB OTP), Speed (Checksum OOB), Certain (Double sending, double OOB checksum)
*/ 
