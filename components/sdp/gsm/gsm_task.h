#ifndef _GSM_TASK_H_
#define _GSM_TASK_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern TaskHandle_t gsm_modem_setup_task;

extern char *operator_name;


int get_sync_attempts();
void gsm_start(char * _log_prefix);
void handle_gsm_states(int state);
void gsm_abort_if_shutting_down();
void gsm_cleanup();

#endif