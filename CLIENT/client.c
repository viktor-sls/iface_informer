/*
 * © Viktor Evdokimov, viktor_sls@mail.ru
 */

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iv.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/socket.h>
#include <sys/un.h>
#include <net/if.h>

#include "../include/common/common.h"

#define ANSWER_TIME_LIMIT 3


struct iv_timer timeout;
struct iv_fd conn;

void print_report( struct iface_util_report *report);
void time_is_out(void *arg);
static void handle_msg(void *cookie);
static void send_request(struct iv_fd *cookie);
void fatal_error_handler(const char *error_msg);

int main(int argc, char *argv[])
{
	if(argc < 2) {
		printf("Error: Path to socket-file is not defined\n");
		return -1;
	}
	char *file_name;
	file_name = calloc( strlen(argv[1]), sizeof(char) );
	strncpy( file_name, argv[1], sizeof(file_name) );

	struct sockaddr_un addr;
	int fd;
	int con_ok;


	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;

	strncpy (addr.sun_path, argv[1], sizeof (addr.sun_path));
	addr.sun_path[ sizeof(addr.sun_path) - 1 ] = '\0';

	iv_init();

	iv_set_fatal_msg_handler(fatal_error_handler);


	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		iv_fatal("%s: socket %s", __func__, strerror(errno));

	con_ok = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	if(con_ok == -1)
		iv_fatal("%s: Connect is failed %s", __func__, strerror(errno));

	IV_FD_INIT(&conn);
	conn.fd = fd;
	conn.cookie = (void *)&conn;
	conn.handler_in = handle_msg;
	conn.handler_out = NULL;
	iv_fd_register(&conn);

	send_request(&conn);

	iv_validate_now();
	IV_TIMER_INIT( &timeout );
	timeout.cookie = NULL;
	timeout.handler = time_is_out;
	timeout.expires = iv_now;
	timeout.expires.tv_sec += ANSWER_TIME_LIMIT;

	iv_timer_register(&timeout);

	iv_main();

	iv_deinit();

	free(file_name);

	return 0;
}


void fatal_error_handler(const char *error_msg)
{
	printf("%s\n", error_msg);
	exit(-1);
}

void print_report( struct iface_util_report *report)
{
	printf("Util for iface %s:\n", report->iface_name);
	printf("\tCurrent stat:\n");
	printf("\tRX packets: %llu \n", report->current_state.rx_packets);
	printf("\tTX packets: %llu \n", report->current_state.tx_packets);
	printf("\tRX bytes: %llu \n", report->current_state.rx_bytes);
	printf("\tTX bytes: %llu \n", report->current_state.tx_bytes);

	printf("\n");

	printf("\tUtil for last 10 sec:\n");
	printf("\tRX: %.5f Mbps\n", report->util_last_10_sec.rx_mbytes_per_sec);
	printf("\tTX: %.5f Mbps\n", report->util_last_10_sec.tx_mbytes_per_sec);
	printf("\n");

	printf("\tUtil for last 30 sec:\n");
	printf("\tRX: %.5f Mbps\n", report->util_last_30_sec.rx_mbytes_per_sec);
	printf("\tTX: %.5f Mbps\n", report->util_last_30_sec.tx_mbytes_per_sec);
	printf("\n");

	printf("\tUtil for last 60 sec:\n");
	printf("\tRX: %.5f Mbps\n", report->util_last_60_sec.rx_mbytes_per_sec);
	printf("\tTX: %.5f Mbps\n", report->util_last_60_sec.tx_mbytes_per_sec);
	printf("\n");
}


static void handle_msg(void *cookie)
{
	iv_timer_unregister(&timeout);

	struct iface_util_report reports[MAX_IFACES];
	struct iv_fd *conn = (struct iv_fd *)cookie;
	int idx;
	int answer_flag = 0;
	ssize_t read_bytes;
	int ifaces_cnt;

	read_bytes = read(conn->fd, reports, ( sizeof(*reports) * MAX_IFACES ) );

	if( read_bytes == -1 )
		iv_fatal("%s: Error when trying to read msg from server %s", __func__, strerror(errno));

	if(( read_bytes % sizeof(*reports) ) != 0 )
		iv_fatal("%s, %d, recieved incorrect data size", __func__, __LINE__);

	ifaces_cnt = ( read_bytes / sizeof(*reports) );

	for(idx = 0; idx < ifaces_cnt; idx++)
		print_report(&reports[idx]);

	iv_fd_unregister(conn);
	close(conn->fd);
}

static void send_request(struct iv_fd *cookie)
{
	struct iv_fd *conn = cookie;
	msg_type_e msg_type = GIVE_ME_DATA;

	if( send(conn->fd, &msg_type, sizeof(msg_type), 0) == -1 )
		iv_fatal("%s: Error when trying to send msg to server %s", __func__, strerror(errno));
}

void time_is_out(void *arg)
{
	close(conn.fd);
	iv_fatal("Server didn't respons for time limit '%d' sec", ANSWER_TIME_LIMIT);
}
