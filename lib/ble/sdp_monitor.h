#ifdef __cplusplus
extern "C"
{
#endif
/**
 * This is a monitor that periodically samples different aspects at runtime.
 * It is used for reporting and detecting and reporting problems, but also for .
 * troubleshooting (especially memory leaks) and optimization.
 * TODO: Question: Should the monitor also be the one that gets samples for the controller?
 * Or is that out of scope? Should that more be a scheduler?
 */

struct monitor {
    problem
}


void init_monitor(char *log_prefix);

int add_monitor()

#ifdef __cplusplus
} /* extern "C" */
#endif