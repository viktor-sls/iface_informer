/*
 * © Viktor Evdokimov, viktor_sls@mail.ru
 */

#include <netlink/addr.h>
#include <netlink/route/link.h>
#include <netlink/socket.h>

#include "server.h"

extern int ifaces_cnt;
extern struct iface_info_instance iface_info_instances[MAX_IFACES];


int get_stats_sample(char *iface_name);
int is_queue_full( struct iface_info_instance *instance );
void update_stat_queue(char *iface_name);
void queue_delete_extra_element( struct iface_info_instance *instance );
struct iface_info_instance * iface_info_instance_find(char *iface_name);

int get_stats_sample(char *iface_name)
{
	struct nl_sock   *sock;
	struct nl_cache  *cache;
	struct rtnl_link *link;
	struct iface_info_instance *instance;

	sock = nl_socket_alloc();
	if (!sock)
		iv_fatal("%s: nl_socket_alloc %s", __func__, strerror(errno));

	nl_connect(sock, NETLINK_ROUTE);

	if ( (rtnl_link_alloc_cache(sock, AF_UNSPEC, &cache)) < 0 )
		iv_fatal("%s: rtnl_link_alloc_cache %s", __func__, strerror(errno));

	if ( !(link = rtnl_link_get_by_name(cache, iface_name)) )
		iv_fatal("%s: rtnl_link_get_by_name %s", __func__, strerror(errno));

	instance = iface_info_instance_find(iface_name);
	if (!instance)
		iv_fatal("%s: instance for iface '%s' is not found", __func__, iface_name);


	instance->curr_iface_stats.rx_packets = rtnl_link_get_stat(link, RTNL_LINK_RX_PACKETS);
	instance->curr_iface_stats.tx_packets = rtnl_link_get_stat(link, RTNL_LINK_TX_PACKETS);

	instance->curr_iface_stats.rx_bytes = rtnl_link_get_stat(link, RTNL_LINK_RX_BYTES);
	instance->curr_iface_stats.tx_bytes = rtnl_link_get_stat(link, RTNL_LINK_TX_BYTES);

	if (link)
		rtnl_link_put(link);

	if (cache)
		nl_cache_put(cache);

	if (sock)
		nl_socket_free(sock);

	return 0;
}



void update_stat_queue(char *iface_name)
{
	struct iface_info_instance *instance;

	instance = iface_info_instance_find(iface_name);
	if (!instance)
		iv_fatal("%s: instance for iface '%s' is not found", __func__, iface_name);
	
	if(is_queue_full(instance) == 1)
		queue_delete_extra_element(instance);

	struct iface_util_state *new_stat_element;
	new_stat_element = calloc( 1, sizeof( struct iface_util_state ) );
	
	new_stat_element->rx_bytes = instance->curr_iface_stats.rx_bytes;
	new_stat_element->tx_bytes = instance->curr_iface_stats.tx_bytes;
	
	if(instance->head_of_stat_queue != NULL)
		new_stat_element->ptr_to_next = instance->head_of_stat_queue;

	instance->head_of_stat_queue = new_stat_element;
}



int is_queue_full( struct iface_info_instance *instance )
{
	int idx = 0;
	struct iface_util_state *current_position = instance->head_of_stat_queue;

	if(instance->head_of_stat_queue != NULL) {
		while(current_position->ptr_to_next != NULL) {
			idx++;
			current_position = current_position->ptr_to_next;
		}
	}
	
	if(idx >= QUEUE_MAX_SIZE)
		return 1;
	else
		return 0;
}


void queue_delete_extra_element( struct iface_info_instance *instance )
{
	struct iface_util_state *current_position = instance->head_of_stat_queue;
	struct iface_util_state *prev_position;

	if(instance->head_of_stat_queue == NULL)
		return;

	while(current_position->ptr_to_next != NULL) {
		prev_position = current_position;
		current_position = current_position->ptr_to_next;
	}

	free(current_position);
	prev_position->ptr_to_next = NULL;
	return;
}


struct iface_info_instance * iface_info_instance_find(char *iface_name)
{
	int idx;
	for(idx = 0; idx < ifaces_cnt; idx++) {
		if(strncmp(iface_info_instances[idx].iface_name, iface_name, sizeof(iface_info_instances[idx].iface_name)) == 0)
			return &iface_info_instances[idx];
	}
	return NULL;
}
