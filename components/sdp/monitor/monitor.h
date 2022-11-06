
/**
 * This is a monitor that periodically samples different aspects at runtime.
 * It is used for reporting and detecting and reporting problems, but also for .
 * troubleshooting (especially memory leaks) and optimization.
 */

#ifndef _SDP_MONITOR_H_
#define _SDP_MONITOR_H_

void sdp_init_monitor(char *_log_prefix);

#endif