/*
 * © Viktor Evdokimov, viktor_sls@mail.ru
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <iv.h>
#include <iv_signal.h>
#include <pthread.h>
#include <sys/un.h>
#include <limits.h>

#include "server.h"


#define NO_CHANGE_CWD_FLAG  1
#define NO_REDIRECT_IO_FLAG 1

char *log_file_path = NULL;
int ifaces_cnt;

pthread_mutex_t head_of_stat_queue_mutex;
struct iv_timer update_timer;
struct iv_signal my_signal;
struct iface_info_instance iface_info_instances[MAX_IFACES];
struct iv_fd listening_socket;
struct iv_timer log_timer;

int check_args(int argc, char *argv[]);
void server_deinit(void);
void server_init(void);
void fatal_error_handler(const char *error_msg);
void init_listen_socket(void);
void init_timers(void);
void init_signals(void);
void init_signal_sighup(void);
void listening_socket_handler(void *_sock);
void connection_handler(void *cookie);
void signal_sighup_handler(void *arg);
void print_border(size_t len);

int main(int argc, char *argv[])
{
	//daemon(NO_CHANGE_CWD_FLAG, NO_REDIRECT_IO_FLAG);

	if(check_args(argc, argv) == RCODE_FAIL)
		return -1;

	log_file_path = argv[1];

	server_init();

	iv_main();

	server_deinit();

	return 0;
}

void server_init(void)
{
	iv_init();

	iv_set_fatal_msg_handler( fatal_error_handler );

	log_open(log_file_path);

	load_config();

	init_listen_socket();

	init_timers();

	init_signals();
	printf("ifaces_cnt = %d\n", ifaces_cnt);
}

void server_deinit(void)
{
	iv_deinit();
	log_close();
}

int check_args(int argc, char *argv[])
{
	if(argc > 2) {
		printf("Error: Too many arguments\n");
		return RCODE_FAIL;
	}

	return RCODE_OK;
}

void fatal_error_handler(const char *error_msg)
{
	printf("%s\n", error_msg);
	log_close();
}

void init_listen_socket(void)
{
	int fd;

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));

	addr.sun_family = AF_UNIX;
	strncpy (addr.sun_path, DEFAULT_SOCK_FILE_NAME, sizeof (addr.sun_path));
	addr.sun_path[ sizeof(addr.sun_path) - 1 ] = '\0';

	fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd < 0)
		iv_fatal("%s: socket %s", __func__, strerror(errno));

	unlink(DEFAULT_SOCK_FILE_NAME);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		iv_fatal("%s: bind %s", __func__, strerror(errno));

	if (listen(fd, 4) < 0)
		iv_fatal("%s: listen %s", __func__, strerror(errno));

	printf( "Pid == %d\n", getpid() );


	IV_FD_INIT(&listening_socket);
	listening_socket.fd = fd;
	listening_socket.cookie = (void *)&listening_socket;
	listening_socket.handler_in = listening_socket_handler;
	iv_fd_register(&listening_socket);
}

void init_timers(void)
{
	iv_validate_now();

	IV_TIMER_INIT( &update_timer );
	update_timer.cookie = NULL;
	update_timer.handler = update_stats;
	update_timer.expires = iv_now;
	update_timer.expires.tv_sec += UPDATE_STATS_PERIOD;
	iv_timer_register(&update_timer);


	IV_TIMER_INIT( &log_timer );
	log_timer.cookie = NULL;
	log_timer.handler = write_stats_to_log_handler;
	log_timer.expires = iv_now;
	log_timer.expires.tv_sec += WRITE_STATS_PERIOD;
	iv_timer_register(&log_timer);
}

void init_signals(void)
{
	init_signal_sighup();
}

void init_signal_sighup(void)
{
	my_signal.signum  = SIGHUP;
	my_signal.handler = signal_sighup_handler;
	my_signal.cookie  = NULL;
	iv_signal_register( &my_signal );
	IV_SIGNAL_INIT( &my_signal );
}

void listening_socket_handler(void *_sock)
{
	struct iv_fd *listening_socket = (struct iv_fd *)_sock;
	struct sockaddr_in addr;
	struct iv_fd *conn;

	socklen_t addrlen = sizeof(addr);

	int fd;
	fd = accept(listening_socket->fd, (struct sockaddr *)&addr, &addrlen);
	if (fd < 0) {
		if (errno == EAGAIN)
			return;

		iv_fatal("%s: accept %s", __func__, strerror(errno));
	}

	conn = calloc(1, sizeof(struct iv_fd));
	if (conn == NULL) {
		printf("listening_socket_handler: memory allocation error, dropping connection\n");
		close(fd);
		return;
	}

	IV_FD_INIT(conn);
	conn->fd = fd;
	conn->cookie = (void *)conn;
	conn->handler_in = connection_handler;
	iv_fd_register(conn);
}

void connection_handler(void *cookie)
{
	struct iv_fd *conn = (struct iv_fd *)cookie;
	msg_type_e msg_type;
	
	int len;
	int idx;
	len = read(conn->fd, &msg_type, sizeof(msg_type));

	if (len <= 0) {
		if (len < 0 && errno == EAGAIN)
			return;
		iv_fd_unregister(conn);
		close(conn->fd);
		free(conn);
		return;
	}

	if(len != sizeof(msg_type) )
		iv_fatal("%s, %d, incorrect command size", __func__, __LINE__);

	if(msg_type == GIVE_ME_DATA) {
		struct iface_util_report *reports;
		reports = calloc( ifaces_cnt, sizeof(struct iface_util_report));

		for(idx = 0; idx < ifaces_cnt; idx++) {
			get_iface_util_report(&reports[idx], &iface_info_instances[idx]);
		}

		ssize_t send_bytes;
		send_bytes = send(conn->fd, reports, (sizeof(*reports) * ifaces_cnt), 0);
		if(send_bytes == -1)
			iv_fatal("%s: send %s", __func__, strerror(errno));

		free(reports);
		iv_fd_unregister(conn);
		close(conn->fd);
		free(conn);
	}
}

void signal_sighup_handler(void *arg)
{
	int idx;
	char head_str[1024];

	printf("\033[2J"); // Clear the entire screen. 
	printf("\n");

	memset(head_str, 0, sizeof(head_str));
	snprintf(head_str, sizeof(head_str), "%-*s %-*s %-*s %-*s %-*s",
	                                     IFNAMSIZ,    "Interfaces",
	                                     NUM_STR_PAD, "RX packets",
	                                     NUM_STR_PAD, "TX packets",
	                                     NUM_STR_PAD, "RX bytes",
	                                     NUM_STR_PAD, "TX bytes");
	printf("%s\n", head_str);
	print_border(strlen(head_str));

	pthread_mutex_lock(&head_of_stat_queue_mutex);

	for(idx = 0; idx < ifaces_cnt; idx++)
		print_iface_util( &iface_info_instances[idx] );

	pthread_mutex_unlock(&head_of_stat_queue_mutex);
}

void print_border(size_t len)
{
	while(len--)
		printf("-");

	printf("\n");
}


