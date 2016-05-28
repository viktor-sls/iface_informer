/*
 * © Viktor Evdokimov, viktor_sls@mail.ru
 */

#ifndef SERVER_H_
#define SERVER_H_

#include <stdio.h>
#include <pthread.h>
#include <sys/un.h>
#include <iv.h>
#include <iv_signal.h>

#include <sys/ioctl.h>
#include <netlink/route/link.h>

#include "../include/common/common.h"

#define NUM_STR_PAD 15

#define QUEUE_MAX_SIZE 60
#define UPDATE_STATS_PERIOD 1
#define WRITE_STATS_PERIOD 1

#define PERIOD_LAST_10_SEC 10
#define PERIOD_LAST_30_SEC 30
#define PERIOD_LAST_60_SEC 60

#define TIMESTAMP_SIZE 256

#define DEFAULT_SOCK_FILE_NAME "/tmp/if_stats_server_sock"
#define DEFAULT_LOG_FILE_NAME  "/tmp/if_stats_server_log"

typedef enum {
	RCODE_OK,
	RCODE_FAIL
} rcode_e;


struct iface_info_instance {
	char iface_name[IFNAMSIZ];
	struct iface_util_state *head_of_stat_queue;
	struct iface_state curr_iface_stats;
};

struct iface_util_state {
	ull rx_bytes;
	ull tx_bytes;

	struct iface_util_state *ptr_to_next;
};

int is_queue_full( struct iface_info_instance *instance );
int get_stats_sample(char *iface_name);
int log_open(char *file_path);
void load_config(void);
void signal_sighup_handler(void *arg);
void write_stats_to_log_handler(void *arg);
void print_iface_util( struct iface_info_instance *instance);
void get_util_by_time(int time_limit, struct iface_util_by_time *info, struct iface_info_instance *instance);
void get_util_for_util_last_10_sec(struct iface_util_by_time *info, struct iface_info_instance *instance);
void get_util_for_util_last_30_sec(struct iface_util_by_time *info, struct iface_info_instance *instance);
void get_util_for_util_last_60_sec(struct iface_util_by_time *info, struct iface_info_instance *instance);
void queue_delete_extra_element( struct iface_info_instance *instance );
void update_stat_queue(char *iface_name);
void update_stats(void *arg);
void fatal_error_handler(const char *error_msg);
void get_iface_util_report(struct iface_util_report *report, struct iface_info_instance *instance);
void log_close(void);
void log_write(const char *fmt, ...);
struct iface_info_instance * iface_info_instance_find(char *iface_name);

#endif // SERVER_H_
