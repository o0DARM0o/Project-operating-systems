#include "logging.h"
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

#include "sub_struct.h"

//When detected CTRL-C the pipe closes and the session is over

static void sig_handler(int sig) {
  // UNSAFE: This handler uses non-async-signal-safe functions (printf(),
  // exit();)
  if (sig == SIGINT) {
    if (signal(SIGINT, sig_handler) == SIG_ERR) {
      exit(EXIT_FAILURE);
    }
  // Must be SIGQUIT - print a message and terminate the process
  fprintf(stderr, "\nCaught SIGQUIT - Session over!\n");
  exit(EXIT_SUCCESS);
    /*return; // Resume execution at point of interruption*/
  }
  return;
}

void read_msg(char *dest, int rx) {
    memset(dest, 0, MAX_SIZE);
    char buffer[BUFFER_SIZE];
    ssize_t ret, red = 0;
    while (true) {
        ret = read(rx, buffer, BUFFER_SIZE - 1);
        if (ret == 0) {
            break;
        } else if (ret == -1) {
            fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        memcpy(dest + red, buffer, (size_t)ret);
        red += ret;
        buffer[ret] = 0;
    }
}

int wait_response(int rx) {
    char str[MAX_SIZE];
    read_msg(str, rx);
    int b;
    memcpy(&b, str + OP_CODE_SIZE, RETURN_CODE_SIZE);
    fprintf(stdout, "sub: A resposta foi \"%d|%d|%s\"\n", (int)str[0], b, str + 5);
    if(b == 0){
        fprintf(stdout, "OK\n");
        return 0;
    }
    return -1;
}

void receive_box_msg(char *cli_pipename) {
    char ant_str[MAX_SIZE];
    memset(ant_str, 0, MAX_SIZE);
    char str[MAX_SIZE];
    int rx = open(cli_pipename, O_RDONLY);
    if (rx == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    read_msg(str, rx);
    
    if (close(rx) == -1) {
        fprintf(stderr, "[ERR]: close failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (memcmp(ant_str, str, MAX_SIZE) != 0) {
        fprintf(stdout, "\"%s\"\n", string_to_box_message(str).msg);
    }
}

int main(int argc, char **argv) {

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        exit(EXIT_FAILURE);
    }
    if (signal(SIGQUIT, sig_handler) == SIG_ERR) {
        exit(EXIT_FAILURE);
    }

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
    if (mkfifo(argv[2], 0640) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        exit(EXIT_FAILURE);
    }

    // send request
    request_msg_t req = request_msg_init(2, argv[2], argv[3]);
    int tx = open(reg_pipe, O_WRONLY);
    if (tx == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    send_request(tx, req);
    printf("sub: Estou a enviar uma mensagem\n");
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

    for(;;) {
    
        receive_box_msg(argv[2]);
    }


    if (unlink(argv[2]) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", argv[2], strerror(errno));
        exit(EXIT_FAILURE);
    }

    return 0;
}