#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../msg.h"
#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define M_PATH "../mbroker/"

/*********************** Message stuff  *********************/
void request_msg_to_string(uint8_t *dest, request_msg_t r) {
    memcpy(dest, &r.op_code, OP_CODE_SIZE);
    memcpy(dest + OP_CODE_SIZE, r.cli_pipename, CLI_PIPENAME_SIZE);
    memcpy(dest + OP_CODE_SIZE + CLI_PIPENAME_SIZE, r.box_name, BOX_NAME_SIZE);
}

//subscriber
request_msg_t request_msg_init(uint8_t code, char* cli_pipe, char* box) {
    request_msg_t r;
    r.op_code = code;
    memset(r.cli_pipename, 0, CLI_PIPENAME_SIZE);
    memset(r.box_name, 0, BOX_NAME_SIZE);
    sprintf(r.cli_pipename, "../manager/%s", cli_pipe);
    strcpy(r.box_name, box);
    return r;
}

/*********************** End ********************************/

void send_request(int tx, request_msg_t r) {
    size_t size = OP_CODE_SIZE + CLI_PIPENAME_SIZE + BOX_NAME_SIZE, written = 0;
    uint8_t buf[size];
    request_msg_to_string(buf, r);
    ssize_t ret;
    while (written < size) { // send request for create/remove or list boxes
        ret = write(tx, buf + written, size - written);
        if (ret < 0) {
            fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        written += (size_t)ret;
    }
}

void wait_response(int rx) {
    char str[MAX_SIZE];
    memset(str, 0, MAX_SIZE);
    char buffer[BUFFER_SIZE];
    ssize_t ret, red = 0;
    while (true) {
        ret = read(rx, buffer, BUFFER_SIZE - 1);   // reads response from mbroker
        if (ret == 0) {
            // ret == 0 indicates EOF  
            break;
        } else if (ret == -1) {
            // ret == -1 indicates error
            fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        memcpy(str + red, buffer, (size_t)ret);
        red += ret;
        buffer[ret] = 0;
    }
    int b;
    memcpy(&b, str + OP_CODE_SIZE, RETURN_CODE_SIZE);
    if(b == 0){ // if the request was succeed will be print OK
        fprintf(stdout, "OK\n");
    }
    //fprintf(stdout, "man: A resposta foi \"%d|%d|%s\"\n", (int)str[0], b, str + 5);
}

int main(int argc, char **argv) {
    
    if(argc < 4 || argc > 5){
        printf("Invalid arguments.\n");
        return -1;
    }

    char function[strlen(argv[3])];
    strcpy(function, argv[3]);

    //save register pipe name
    char reg_pipe[strlen(argv[1]) + 11];
    sprintf(reg_pipe, "%s%s", M_PATH, argv[1]);

    request_msg_t req;
    //get op_code
    if (strcmp(function, "list") == 0) {
        req = request_msg_init(7, argv[2], "\0");
    } else if (argc == 5) {
        if(strcmp(function, "create") == 0) {
            req = request_msg_init(3, argv[2], argv[4]);
        } else if (strcmp(function, "remove") == 0) {
            req = request_msg_init(5, argv[2], argv[4]);
        } else {
            //erro
            fprintf(stderr, "[ERR]: invalid command: \"%s\"\n", argv[3]);
            exit(EXIT_FAILURE);
        }
    } else {
        //erro
        fprintf(stderr, "[ERR]: invalid arguments\n");
        exit(EXIT_FAILURE);
    }

    // Create pipe
    if (unlink(argv[2]) != 0 && errno != ENOENT) {  //client pipe
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", argv[2], strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (mkfifo(argv[2], 0640) != 0) {    //client pipe
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // send request
    int tx = open(reg_pipe, O_WRONLY);
    if (tx == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    send_request(tx, req);
    //printf("sub: Estou a enviar uma mensagem\n");
    if (close(tx) == -1) {
        fprintf(stderr, "[ERR]: close failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // open pipe
    int rx = open(argv[2], O_RDONLY);
    if (rx == -1 ) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // wait response
    wait_response(rx);
    if (close(rx) == -1) {
        fprintf(stderr, "[ERR]: close failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (unlink(argv[2]) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", argv[2], strerror(errno));
        exit(EXIT_FAILURE);
    }
    return 0;
}