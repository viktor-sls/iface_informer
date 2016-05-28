/*
 * © Viktor Evdokimov, viktor_sls@mail.ru
 */

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <limits.h>

#include "server.h"

FILE *log_file = NULL;
char *log_file_path_default = "/tmp/logfile";
extern int ifaces_cnt;
extern struct iface_info_instance iface_info_instances[MAX_IFACES];


struct iv_timer log_timer;



void get_timestamp(char *buf, size_t buf_size)
{
	struct timeval tv;
	struct tm *local_time;

	gettimeofday(&tv, NULL);

	local_time = localtime(&tv.tv_sec);

	memset(buf, 0, buf_size);
	strftime(buf, buf_size, "%d-%m-%Y %k:%M:%S", local_time);
}

int log_open(char *file_path)
{
	if (!file_path) {
		file_path = log_file_path_default;
		printf("Log file is not defined. Using default file: %s\n", file_path);
	}

	if( access(file_path, F_OK) == 0)
		unlink(file_path);

	log_file = fopen(file_path, "a");
	if(log_file == NULL) {
		iv_fatal("%s, fopen failed: %s\n", __func__, strerror(errno));
	}

	return 0;
}

void log_close(void)
{
	if (log_file)
		fclose(log_file);
}

void log_write(const char *fmt, ...)
{
	va_list arg_list;

	va_start(arg_list, fmt);

	vfprintf(log_file, fmt, arg_list);
	fflush(log_file);

	va_end(arg_list);
}


void write_stats_to_log_handler(void *arg)
{
	int idx;
	struct iface_util_by_time info;
	char timestamp[TIMESTAMP_SIZE];

	get_timestamp(timestamp, sizeof(timestamp));

	log_write("=========================================================================\n");
	log_write("TIMESTAMP: %s\n", timestamp);
	for( idx = 0; idx < ifaces_cnt; idx++ ) {
		log_write("Interface %s:\n", iface_info_instances[idx].iface_name);

		get_util_for_util_last_10_sec(&info, &iface_info_instances[idx]);
		log_write("\tUtilization for last 10 sec:  ");
		log_write(" RX: %.5f Mbps,", info.rx_mbytes_per_sec);
		log_write(" TX: %.5f Mbps\n", info.tx_mbytes_per_sec);

		get_util_for_util_last_30_sec(&info, &iface_info_instances[idx]);
		log_write("\tUtilization for last 30 sec:  ");
		log_write(" RX: %.5f Mbps,", info.rx_mbytes_per_sec);
		log_write(" TX: %.5f Mbps\n", info.tx_mbytes_per_sec);

		get_util_for_util_last_60_sec(&info, &iface_info_instances[idx]);
		log_write("\tUtilization for last 60 sec:  ");
		log_write(" RX: %.5f Mbps,", info.rx_mbytes_per_sec);
		log_write(" TX: %.5f Mbps\n", info.tx_mbytes_per_sec);
		log_write("\n");
	}
	log_write("=========================================================================\n");

	iv_validate_now();

	log_timer.expires = iv_now;
	log_timer.expires.tv_sec += WRITE_STATS_PERIOD;
	iv_timer_register(&log_timer);
}
