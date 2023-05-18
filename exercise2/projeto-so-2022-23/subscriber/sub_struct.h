#include "../msg.h"

void send_request(int tx, request_msg_t r);


typedef struct {
	uint8_t op_code;
	char msg[MESSAGE_SIZE];
} box_message_t;

box_message_t box_message_init(uint8_t op_c, char *box_msg);
box_message_t string_to_box_message(char *str);