/*
 * © Viktor Evdokimov, viktor_sls@mail.ru
 */

#include <iv.h>
#include <iv_signal.h>

#include "server.h"

extern int ifaces_cnt;
extern pthread_mutex_t head_of_stat_queue_mutex;
extern struct iv_timer update_timer;
extern struct iface_info_instance iface_info_instances[MAX_IFACES];

void get_iface_util_report(struct iface_util_report *report, struct iface_info_instance *instance);
void print_iface_util( struct iface_info_instance *instance);
void get_util_by_time(int time_limit, struct iface_util_by_time *info, struct iface_info_instance *instance);
void get_util_for_util_last_10_sec(struct iface_util_by_time *info, struct iface_info_instance *instance);
void get_util_for_util_last_30_sec(struct iface_util_by_time *info, struct iface_info_instance *instance);
void get_util_for_util_last_60_sec(struct iface_util_by_time *info, struct iface_info_instance *instance);
void update_stats(void *arg);

void get_iface_util_report(struct iface_util_report *report, struct iface_info_instance *instance)
{
	pthread_mutex_lock(&head_of_stat_queue_mutex);
	strncpy(report->iface_name,
	        instance->iface_name,
	        sizeof(report->iface_name));

	memcpy(&report->current_state,
	       &instance->curr_iface_stats,
	       sizeof(report->current_state));
	pthread_mutex_unlock(&head_of_stat_queue_mutex);


	get_util_for_util_last_10_sec(&report->util_last_10_sec, instance);
	get_util_for_util_last_30_sec(&report->util_last_30_sec, instance);
	get_util_for_util_last_60_sec(&report->util_last_60_sec, instance);
}

void print_iface_util( struct iface_info_instance *instance)
{
	printf("%-*s %-*llu %-*llu %-*llu %-*llu\n",
	       IFNAMSIZ,    instance->iface_name,
	       NUM_STR_PAD, instance->curr_iface_stats.rx_packets,
	       NUM_STR_PAD, instance->curr_iface_stats.tx_packets,
	       NUM_STR_PAD, instance->curr_iface_stats.rx_bytes,
	       NUM_STR_PAD, instance->curr_iface_stats.tx_bytes);

	return;
}

void get_util_by_time(int time_limit, struct iface_util_by_time *info, struct iface_info_instance *instance)
{
	int idx = 1;

	ull sum_rx_bytes = 0;
	ull sum_tx_bytes = 0;

	double result_rx_mbytes;
	double result_tx_mbytes;

	struct iface_util_state *current_position;

	pthread_mutex_lock(&head_of_stat_queue_mutex);
	
	current_position = instance->head_of_stat_queue;

	if(instance->head_of_stat_queue == NULL) {
		printf("Util for last %d is N/A\n", time_limit);
		return;
	}

	while( (current_position->ptr_to_next != NULL) && (idx != time_limit) ){
		sum_tx_bytes += ( current_position->tx_bytes - current_position->ptr_to_next->tx_bytes );
		sum_rx_bytes += ( current_position->rx_bytes - current_position->ptr_to_next->rx_bytes );
		current_position = current_position->ptr_to_next;
		idx++;
	}

	result_tx_mbytes = sum_tx_bytes / (double)idx * 8 / 1000 / 1000;
	result_rx_mbytes = sum_rx_bytes / (double)idx * 8 / 1000 / 1000;

	info->rx_mbytes_per_sec = result_rx_mbytes;
	info->tx_mbytes_per_sec = result_tx_mbytes;

	pthread_mutex_unlock(&head_of_stat_queue_mutex);
}

void get_util_for_util_last_10_sec(struct iface_util_by_time *info, struct iface_info_instance *instance)
{
	get_util_by_time(PERIOD_LAST_10_SEC, info, instance);
}

void get_util_for_util_last_30_sec(struct iface_util_by_time *info, struct iface_info_instance *instance)
{
	get_util_by_time(PERIOD_LAST_30_SEC, info, instance);
}

void get_util_for_util_last_60_sec(struct iface_util_by_time *info, struct iface_info_instance *instance)
{
	get_util_by_time(PERIOD_LAST_60_SEC, info, instance);
}

void update_stats(void *arg)
{
	int idx;

	pthread_mutex_lock(&head_of_stat_queue_mutex);

	for(idx = 0; idx < ifaces_cnt; idx++) {
		if(get_stats_sample(iface_info_instances[idx].iface_name) == 1)
			continue;
		update_stat_queue(iface_info_instances[idx].iface_name);
	}

	pthread_mutex_unlock(&head_of_stat_queue_mutex);

	iv_validate_now();

	update_timer.expires = iv_now;
	update_timer.expires.tv_sec += UPDATE_STATS_PERIOD;
	iv_timer_register(&update_timer);
}
