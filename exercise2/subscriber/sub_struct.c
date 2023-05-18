#include "sub_struct.h"

void request_msg_to_string(uint8_t *dest, request_msg_t r) {
    memcpy(dest, &r.op_code, OP_CODE_SIZE);
    memcpy(dest + OP_CODE_SIZE, r.cli_pipename, CLI_PIPENAME_SIZE);
    memcpy(dest + OP_CODE_SIZE + CLI_PIPENAME_SIZE, r.box_name, BOX_NAME_SIZE);
}
request_msg_t request_msg_init(uint8_t code, char* cli_pipe, char* box) {
    request_msg_t r;
    r.op_code = code;
    memset(r.cli_pipename, 0, CLI_PIPENAME_SIZE);
    memset(r.box_name, 0, BOX_NAME_SIZE);
    sprintf(r.cli_pipename, "../subscriber/%s", cli_pipe);
    strcpy(r.box_name, box);
    return r;
}


void send_request(int tx, request_msg_t r) {
    size_t size = OP_CODE_SIZE + CLI_PIPENAME_SIZE + BOX_NAME_SIZE, written = 0;
    uint8_t buf[size];
    request_msg_to_string(buf, r);
    ssize_t ret;
    while (written < size) {
        ret = write(tx, buf + written, size - written);
        if (ret < 0) {
            fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        written += (size_t)ret;
    }
}


box_message_t box_message_init(uint8_t op_c, char *box_msg) {
	box_message_t bm;
	bm.op_code = op_c;
	memcpy(bm.msg, box_msg, MESSAGE_SIZE);
	return bm;
}
box_message_t string_to_box_message(char *str) {
    fprintf(stdout, "\n%d\n", (int)str[0]);
	if (str[0] != 10) {
            fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
	return box_message_init((uint8_t)str[0], str + OP_CODE_SIZE);
}