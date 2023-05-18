#include "../utils/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE (128)

#define OP_CODE_SIZE (1)
#define CLI_PIPENAME_SIZE (256)
#define BOX_NAME_SIZE (32)

#define RETURN_CODE_SIZE (4)
#define MESSAGE_SIZE (1024)
#define ERROR_MESSAGE_SIZE (1024)
#define N_PUB_SIZE (8)

#define MAX_SIZE (1036)

typedef struct {
	uint8_t  op_code;
	char cli_pipename[CLI_PIPENAME_SIZE];
	char box_name[BOX_NAME_SIZE];
	char message[MESSAGE_SIZE];
} request_msg_t;

typedef struct {
	uint8_t op_code;
	int32_t return_code;
	char error_message[ERROR_MESSAGE_SIZE];
} response_t;

typedef struct {
	uint8_t op_code;
	char message[MESSAGE_SIZE];
	char box_name[BOX_NAME_SIZE];
} message_t;

typedef struct {
	uint8_t op_code;
	uint8_t last;
	char box_name[BOX_NAME_SIZE];
	uint64_t box_size;
	uint64_t n_publishers;
	uint64_t n_subscribers;
} list_response_t;

void request_msg_to_string(uint8_t *dest, request_msg_t r);
request_msg_t string_to_request_msg(char *msg);
request_msg_t request_msg_init(uint8_t code, char* cli_pipe, char* box);


void response_to_string(uint8_t *dest, response_t r);
response_t response_init(uint8_t op_c, int32_t response_c, char * error_msg);
response_t format_response(request_msg_t r);


void list_response_to_string(uint8_t *dest, list_response_t lr);
list_response_t list_response_init(uint8_t op_c, uint8_t last_box, char* b_name,
				uint64_t b_size, uint64_t n_pubs, uint64_t n_subs);
list_response_t format_list_response(request_msg_t r);

message_t message_init(uint8_t op_c, char *message);