/*
 * © Viktor Evdokimov, viktor_sls@mail.ru
 */

#include <unistd.h>
#include <cjson/cJSON.h>
#include <limits.h>

#include "server.h"

FILE *config_file = NULL;

int ifaces_cnt = 0;
char *config_buffer = NULL;

cJSON *root;

extern char path_to_config[PATH_MAX];
extern struct iface_info_instance iface_info_instances[MAX_IFACES];

int json_get_root(void);
int json_parsing(void);
void check_ifaces(void);
void check_ifaces_duplication(void);
void check_ifaces_existance(void);

#define PATH_TO_CONFIG "config.json"


void load_config(void)
{
	if(json_get_root() == RCODE_FAIL)
		iv_fatal("Failed at load_config: json_get_root");

	if(json_parsing() == RCODE_FAIL)
		iv_fatal("Failed at load_config: json_get_root");

	free(config_buffer);

	check_ifaces();

	free(root);
}

int json_get_root(void)
{
	int file_size;

	config_file = fopen(PATH_TO_CONFIG, "r");

	if(config_file == NULL) {
		printf("%s, fopen failed: %s\n", __func__, strerror(errno));
		return RCODE_FAIL;
	}

	fseek(config_file, 0, SEEK_END);
	file_size = ftell(config_file);
	fseek(config_file, 0, SEEK_SET);

	config_buffer = calloc(1, file_size);
	if(config_buffer == NULL) {
		printf("calloc failed: %s\n", strerror(errno));
		fclose(config_file);
		return RCODE_FAIL;
	}

	fread(config_buffer, file_size, 1, config_file);
	if(ferror(config_file) != 0 ) {
		printf("failed to read from file\n");
		fclose(config_file);
		free(config_buffer);
		return RCODE_FAIL;
	}
	fclose(config_file);

	root = cJSON_Parse(config_buffer);
	if(root == NULL) {
		printf("cannot get root JSON object\n");
		free(config_buffer);
		return RCODE_FAIL;
	}

	return RCODE_OK;
}


int json_parsing(void)
{
	int idx;
	cJSON *interface_json;
	cJSON *interfaces_json;

	interfaces_json = cJSON_GetObjectItem(root, "interfaces");
	if(interfaces_json == NULL) {
		printf("cannot get interfaces_json JSON object\n");
		free(config_buffer);
		return RCODE_FAIL;
	}

	idx = 0;
	while( (interface_json = cJSON_GetArrayItem(interfaces_json, idx)) != NULL ) {
		if(idx >= MAX_IFACES) {
			printf("Too many interfaces, max number is %d\n", MAX_IFACES);
			free(config_buffer);
			return RCODE_FAIL;
		}

		strncpy( iface_info_instances[idx].iface_name, interface_json->string, sizeof(iface_info_instances[idx].iface_name) );
		idx++;
	}
	
	ifaces_cnt = idx;
	if(ifaces_cnt == 0) {
		printf("No ifaces configured\n");
		free(config_buffer);
		return RCODE_FAIL;
	}

	free(interface_json);
	free(interfaces_json);

	return RCODE_OK;
}

void check_ifaces(void)
{
	check_ifaces_duplication();
	check_ifaces_existance();
}

void check_ifaces_duplication(void)
{
	int curr_idx;
	int next_idx;
	int r_code = 0;
	char *current_iface_name;

	for(curr_idx = 0; curr_idx < ifaces_cnt - 1; curr_idx++) {
		current_iface_name = iface_info_instances[curr_idx].iface_name;

		for(next_idx = curr_idx + 1; next_idx < ifaces_cnt; next_idx++) {
			if ( strcmp( current_iface_name, iface_info_instances[next_idx].iface_name) == 0 ) {
				printf("Iface '%s' is already entered\n", iface_info_instances[next_idx].iface_name);
				r_code = 1;
				break;
			}
		}
	}

	if(r_code != 0)
		iv_fatal("You entered one or more same iface names");
}

void check_ifaces_existance(void)
{
	struct nl_sock   *sock;
	struct nl_cache  *cache;
	int idx;
	int r_code = 0;

	sock = nl_socket_alloc();
	if (!sock)
		iv_fatal("%s: nl_socket_alloc %s", __func__, strerror(errno));

	nl_connect(sock, NETLINK_ROUTE);

	if ( (rtnl_link_alloc_cache(sock, AF_UNSPEC, &cache)) < 0 )
		iv_fatal("%s: rtnl_link_alloc_cache %s", __func__, strerror(errno));

	for(idx = 0; idx < ifaces_cnt; idx++) {
		if ( rtnl_link_get_by_name(cache, iface_info_instances[idx].iface_name) == NULL ) {
			printf("Iface '%s' is not found\n", iface_info_instances[idx].iface_name);
			r_code = 1;
		}
	}

	if(r_code != 0)
		iv_fatal("One or more interfaces is not found");
}

