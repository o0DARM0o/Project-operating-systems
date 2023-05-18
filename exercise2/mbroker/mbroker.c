#include "logging.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../fs/operations.h"
#include "boxes.h"
#include "sessions.h"

#include "../msg.h"
#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int MAX_SESSIONS;

//When detected CTRL-C the pipe closes and the session is over

static void sig_handler(int sig) {
  // UNSAFE: This handler uses non-async-signal-safe functions (printf(),
  // exit();)
  if (sig == SIGINT) {
    if (signal(SIGINT, sig_handler) == SIG_ERR) {
      exit(EXIT_FAILURE);
    }
  // Must be SIGQUIT - print a message and terminate the process
  fprintf(stderr, "\nCaught SIGQUIT - System Down\n");
  exit(EXIT_SUCCESS);
    /*return; // Resume execution at point of interruption*/
  }
  return;
}

/*********************** Message stuff  *********************/
request_msg_t string_to_request_msg(char *msg) {
    request_msg_t r;
    memcpy(&r.op_code, msg, OP_CODE_SIZE);
    memcpy(r.cli_pipename, msg + OP_CODE_SIZE, CLI_PIPENAME_SIZE);
    memcpy(r.box_name, msg + OP_CODE_SIZE + CLI_PIPENAME_SIZE, BOX_NAME_SIZE);
    if(r.op_code == 9){
        memcpy(r.message, msg + OP_CODE_SIZE + CLI_PIPENAME_SIZE + BOX_NAME_SIZE, MESSAGE_SIZE );
    }
    return r;
}

message_t message_init(uint8_t op_c, char *message){
    message_t m;
    m.op_code = op_c;
    memset(m.message, 0, MESSAGE_SIZE);
    sprintf(m.message, "%s", message);
    return m;
}

void response_to_string(uint8_t *dest, response_t r) {
    memcpy(dest, &r.op_code, OP_CODE_SIZE);
    memcpy(dest + OP_CODE_SIZE, &r.return_code, RETURN_CODE_SIZE);
    memcpy(dest + OP_CODE_SIZE + RETURN_CODE_SIZE, r.error_message, ERROR_MESSAGE_SIZE);
}
response_t response_init(uint8_t op_c, int32_t return_c, char* error_msg) {
    response_t r;
    r.op_code = op_c;
    r.return_code = return_c;
    memset(r.error_message, 0, ERROR_MESSAGE_SIZE);
    sprintf(r.error_message, "%s", error_msg);
    return r;
}
response_t format_response(request_msg_t r) { 
    response_t res;
    pthreads_t threads = pthreads_init(MAX_SESSIONS);
    if (r.op_code == 3) {  // OP_CODE to create a box
        int response = create_box(r.box_name);    // 0 if we created the box and -1 if not
        res = response_init(4, response, "\0"); 
    } else if (r.op_code == 5) {// OP_CODE to remove a box
        int response = remove_box(r.box_name);  // 0 if we removed the box and -1 if not
        res = response_init(4, response, "\0");
    } else if(r.op_code == 1|| r.op_code == 2 ){ // OP_CODE to publisher and subcriber
        if(MAX_SESSIONS != 0){  // we only creat a session if the MAX_SESSIOS is diffrent of 0
            if(does_box_exist(r.box_name)){
                res = response_init(r.op_code , 0, "\0");   // if the box exist we can creat a new clint session
                if(r.op_code == 1){
                    pthreads_add(threads, r.cli_pipename, PUB , r.box_name);  // publisher thread
                }else if(r.op_code == 2){
                    pthreads_add(threads, r.cli_pipename, SUB , r.box_name);  // subscriber thread
                }
                MAX_SESSIONS--;
            }else{
                res = response_init(r.op_code , -1, "\0");  //else we send -1 to close the client pipe
            }
        }else{
            res = response_init(r.op_code , -1, "\0");
        }
    } else {
        fprintf(stderr, "[ERR]: invalid code: %c\n", r.op_code);
        exit(EXIT_FAILURE);
    }
    return res;
}

void list_response_to_string(uint8_t *dest, list_response_t r) {
    memcpy(dest, &r.op_code, OP_CODE_SIZE);
    memcpy(dest + OP_CODE_SIZE, &r.last, OP_CODE_SIZE);
    memcpy(dest + 2 * OP_CODE_SIZE, r.box_name, BOX_NAME_SIZE);
    memcpy(dest + 2 * OP_CODE_SIZE + BOX_NAME_SIZE, &r.box_size, N_PUB_SIZE);
    memcpy(dest + 2 * OP_CODE_SIZE + BOX_NAME_SIZE + N_PUB_SIZE, &r.n_publishers, N_PUB_SIZE);
    memcpy(dest + 2 * OP_CODE_SIZE + BOX_NAME_SIZE + 2 * N_PUB_SIZE, &r.n_subscribers, N_PUB_SIZE);
}
list_response_t list_response_init(uint8_t op_c, uint8_t last_box, char* b_name,
                uint64_t b_size, uint64_t n_pubs, uint64_t n_subs) {
    list_response_t lr;
    lr.op_code = op_c;
    lr.last = last_box;
    memset(lr.box_name, 0, BOX_NAME_SIZE);
    strcpy(lr.box_name, b_name);
    lr.box_size = b_size;
    lr.n_publishers = n_pubs;
    lr.n_subscribers = n_subs;
    return lr;
}
list_response_t format_list_response(request_msg_t r) {
    if (r.op_code != 7) {
        fprintf(stderr, "[ERR]: format_list_response: invalid op_code: %d\n", r.op_code);
        exit(EXIT_FAILURE);
    }
    return list_response_init(8, 66, "box_name_1", 66, 6, 6);
}
/*********************** End ********************************/

void getClientPipe(char * dest, request_msg_t r) {
    memcpy(dest, r.cli_pipename, CLI_PIPENAME_SIZE);
}

void config_response(char *dest, request_msg_t r) {
    memset(dest, 0, MAX_SIZE);
    uint8_t c = r.op_code;
    if (c == 1 || c == 2) {
        response_to_string((uint8_t*)dest, format_response(r)); // takes response to string to send to client
    } else if (c == 3 || c == 5) {
        response_to_string((uint8_t*)dest, format_response(r));
    } else if (c == 7) {
        list_response_to_string((uint8_t*)dest, format_list_response(r));
    } /*else if( c == 12){ 
        MAX_SESSIONS++;
    }*/ else {
       fprintf(stderr, "[ERR]: config_response: invalid op_code: %d\n", (int)c);
        exit(EXIT_FAILURE); 
    }
}

void code_receiver(char *dest, int rx) {
    char buffer[BUFFER_SIZE], str[OP_CODE_SIZE + CLI_PIPENAME_SIZE + BOX_NAME_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    memset(str, 0, OP_CODE_SIZE + CLI_PIPENAME_SIZE + BOX_NAME_SIZE);
    ssize_t ret, red = 0;
    while (true) {
        ret = read(rx, buffer, BUFFER_SIZE - 1); // reads the code (request) received from client 
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
        fprintf(stdout, "mbroker: Recebi %ld bytes\n", ret);
    }
    fprintf(stdout, "mbroker: Mensagem total \"%d|%s|%s\"\n", str[0], str + 1, str + 257);
    memcpy(dest, str, OP_CODE_SIZE + CLI_PIPENAME_SIZE + BOX_NAME_SIZE);
}


void send_response(int tx, char* res) {
    char msg[MAX_SIZE];
    memcpy(msg, res, MAX_SIZE);
    ssize_t ret;
    size_t written = 0;
    while (written < MAX_SIZE) { // send response to client
        ret = write(tx, msg + written, MAX_SIZE - written);
        if (ret < 0) {
            fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        written += (size_t)ret;
    }
}

int main(int argc, char **argv) {

    if(argc == 1){
        printf("Without arguments: Pipe's name and max server's sessions.\n ");
        return -1;
    }
    // Only the pipes's name was given
    if(argc == 2){
        printf("Missing max server's session.\n");
        return -1;
    }

    tfs_init(NULL); // TencicoFS init

    //save pipes's name
    char pipename[strlen(argv[1])];
    strcpy(pipename, argv[1]);
    MAX_SESSIONS = atoi(argv[2]);

    if (unlink(pipename) != 0 && errno != ENOENT) {
    // Create pipe
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", pipename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (mkfifo(pipename, 0640) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        exit(EXIT_FAILURE);
    }

    char res_msg[MAX_SIZE];
    memset(res_msg, 0, MAX_SIZE);
    //looping until we detected CTRL-C
    for (;;) {
        int rx = open(pipename, O_RDONLY);
        if (rx == -1 ) {
            fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        //receive the request from client
        code_receiver(res_msg, rx);
        if (close(rx) == -1) {
            fprintf(stderr, "[ERR]: close failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        char cliPipe[CLI_PIPENAME_SIZE];
        request_msg_t req = string_to_request_msg(res_msg);
        config_response(res_msg, req); // prepares the response to send
        if(req.op_code != 9){
            getClientPipe(cliPipe, req);
            int tx = open(cliPipe, O_WRONLY);
            if (tx == -1 ) {
                fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            send_response(tx, res_msg); // send feedback to client
            if (close(tx) == -1) {
                fprintf(stderr, "[ERR]: close failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
        }

    }
    if (unlink(pipename) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", pipename, strerror(errno));
        exit(EXIT_FAILURE);
    }
    tfs_destroy();
    return 0;
}