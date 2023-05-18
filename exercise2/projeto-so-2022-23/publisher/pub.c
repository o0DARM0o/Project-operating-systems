#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
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

/* code = 12 - eliminar sessao*/

/*********************** Message stuff  *********************/
request_msg_t request_msg_init(uint8_t code, char* cli_pipe, char* box) {
    request_msg_t r;
    r.op_code = code;
    memset(r.cli_pipename, 0, CLI_PIPENAME_SIZE);
    memset(r.box_name, 0, BOX_NAME_SIZE);
    sprintf(r.cli_pipename, "../publisher/%s", cli_pipe);
    strcpy(r.box_name, box);
    return r;
}

message_t message_init(uint8_t op_c, char *message){
    message_t m;
    m.op_code = op_c;
    memset(m.message, 0, MESSAGE_SIZE);
    memcpy(m.message, message, strlen(message));
    return m;
}

void request_msg_to_string(uint8_t *dest, request_msg_t r) {
    memcpy(dest, &r.op_code, OP_CODE_SIZE);
    memcpy(dest + OP_CODE_SIZE, r.cli_pipename, CLI_PIPENAME_SIZE);
    memcpy(dest + OP_CODE_SIZE + CLI_PIPENAME_SIZE, r.box_name, BOX_NAME_SIZE);
}

void message_to_string(uint8_t *dest, message_t m){
    memcpy(dest, &m.op_code, OP_CODE_SIZE);
    memcpy(dest + OP_CODE_SIZE, m.message, MESSAGE_SIZE);
}

void send_message(int tx, message_t m){
    size_t size = OP_CODE_SIZE + MESSAGE_SIZE, written = 0;
    uint8_t buf[size];
    message_to_string(buf, m);
    ssize_t ret;
    while (written < size) {  // send the message to mbroker
        ret = write(tx, buf + written, size - written);
        if (ret < 0) {
            fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        written += (size_t)ret;
    }
}
/*********************** End ********************************/


void send_request(int tx, request_msg_t r) {
    size_t size = OP_CODE_SIZE + CLI_PIPENAME_SIZE + BOX_NAME_SIZE, written = 0;
    uint8_t buf[size];
    request_msg_to_string(buf, r);
    ssize_t ret;
    while (written < size) {
        ret = write(tx, buf + written, size - written);  //send requesto to mbroker for new session
        if (ret < 0) {
            fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        written += (size_t)ret;
    }
}

int wait_response(int rx) {
    char str[MAX_SIZE];
    memset(str, 0, MAX_SIZE);
    char buffer[BUFFER_SIZE];
    ssize_t ret, red = 0;
    while (true) {
        ret = read(rx, buffer, BUFFER_SIZE - 1); // reads the response from mbroker
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
    fprintf(stdout, "pub: A resposta foi \"%d|%d|%s\"\n", (int)str[0], b, str + 5);
    if(b == 0){  // if b == 0 means that mbroker permirted a new session
        fprintf(stdout, "OK\n");
        return 0;
    }
    return -1;
}

/****************************************************/

//When detected CTRL-C the pipe closes and the session is over

static void sig_handler(int sig) {

  // UNSAFE: This handler uses non-async-signal-safe functions (printf(),
  // exit();)
  if (sig == SIGINT) {
    if (signal(SIGINT, sig_handler) == SIG_ERR) {
      exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Caught SIGINT\n");
  // Must be SIGQUIT - print a message and terminate the process
  fprintf(stderr, "Caught SIGQUIT - Session over!\n");
  exit(EXIT_SUCCESS);
    /*return; // Resume execution at point of interruption*/
  }
  return;

}
/****************************************************/


int main(int argc, char **argv) {

    if(argc < 4) {
        printf("Invalid arguments.\n");
        return -1;
    }

    //save register's pipename
    char reg_pipe[11 + strlen(argv[1])];
    sprintf(reg_pipe, "../mbroker/%s", argv[1]);

    // Create pipe
    
    if (unlink(argv[2]) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", argv[2], strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (mkfifo(argv[2], 0640) != 0) { //client pipe
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        exit(EXIT_FAILURE);
    }

    // send request to mbroker
    request_msg_t req = request_msg_init(1, argv[2], argv[3]);
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

    // open client pipe for reading so it could read the response of the request
    int rx = open(argv[2], O_RDONLY);
    if (rx == -1 ) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // wait response from mbroker to register client
    int result = wait_response(rx);
    if(result != 0){
        if (close(rx) == -1) {
            fprintf(stderr, "[ERR]: close failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (unlink(argv[2]) != 0 && errno != ENOENT) {
            fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", argv[2], strerror(errno));
            exit(EXIT_FAILURE);
        }  

        exit(EXIT_FAILURE);      

    }

    if (close(rx) == -1) {
        fprintf(stderr, "[ERR]: close failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    // looping wainting the publisher write a message at stdin to write at box
    for(;;){
        char str[MAX_SIZE];
        if(scanf("%s", str ) == 1){
            printf("pub: %s\n", str);
            // message received, so we can send to mbroker
            message_t  message = message_init(9, str);         //code 9 to send the message to mbroker
            int tx2 = open(argv[2], O_WRONLY);  // open client pipe to write the message for mbroker
            if (tx2 == -1) {
                fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            send_message(tx2, message);  //sendind message
            //printf("sub: Estou a enviar uma mensagem\n");
            if (close(tx2) == -1) {
                fprintf(stderr, "[ERR]: close failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
    }


    if (unlink(argv[2]) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", argv[2], strerror(errno));
        exit(EXIT_FAILURE);
    }
    return 0;
}