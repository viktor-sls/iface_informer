/*
 * © Viktor Evdokimov, viktor_sls@mail.ru
 */

#define MAX_IFACES 100

typedef enum {
              GIVE_ME_DATA
             } msg_type_e;



typedef unsigned long long ull;

struct iface_state {
	ull rx_packets;
	ull tx_packets;
	ull rx_bytes;
	ull tx_bytes;
};

struct iface_util_by_time {
	double rx_mbytes_per_sec;
	double tx_mbytes_per_sec;
};

struct iface_util_report {
	char iface_name[IFNAMSIZ];
	struct iface_state current_state;

	struct iface_util_by_time util_last_10_sec;
	struct iface_util_by_time util_last_30_sec;
	struct iface_util_by_time util_last_60_sec;
};

