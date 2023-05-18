#include "logging.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
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


void send_to_pipe(int tx, char* str) {
    size_t size = MESSAGE_SIZE + OP_CODE_SIZE, written = 0;
	ssize_t ret;
	uint8_t buf[size];
	memcpy(buf, str, size);
    while (written < size) {
        ret = write(tx, buf + written, size - written);
        if (ret < 0) {
            fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        written += (size_t)ret;
    }
}

void message_to_string(uint8_t *dest, message_t m){
    memcpy(dest, &m.op_code, OP_CODE_SIZE);
    memcpy(dest + OP_CODE_SIZE, m.message, MESSAGE_SIZE);
}

cli_args_t cli_args_init(char *cli_pipe, char *b_name) {
	cli_args_t args;
	memcpy(args.cli_pipename, cli_pipe, CLI_PIPENAME_SIZE);
	memcpy(args.box_name, b_name, BOX_NAME_SIZE);
	return args;
}

void *sub_thr_func(void* arg) {  // thread to follow subscriber session
	char msg[MESSAGE_SIZE + OP_CODE_SIZE];
	char ant_msg[MESSAGE_SIZE + OP_CODE_SIZE];
	//op_code
	msg[0] = 10;
	memset(msg + 1, 0, MESSAGE_SIZE);
	memset(ant_msg, 0, MESSAGE_SIZE + OP_CODE_SIZE);
	cli_args_t *args = (cli_args_t *)arg;
	int tx;
	while (does_box_exist(args->box_name)) { // while existing the box client, the thread will be readind the box and updating the subscriber
		tx = open(args->cli_pipename, O_WRONLY);
		if (tx == -1 ) {
			fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		
		read_box(msg + 1, args->box_name);
		if (memcmp(msg, ant_msg, MESSAGE_SIZE + OP_CODE_SIZE) != 0) {
			send_to_pipe(tx, msg);
			memcpy(ant_msg, msg, MESSAGE_SIZE + OP_CODE_SIZE);
		}
		
		if (close(tx) == -1) {
			fprintf(stderr, "[ERR]: close failed: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	unlink(args->cli_pipename);
	return 0;
}

void *pub_thr_func(void* arg) { // thread to follow publisher session 
	char msg[MESSAGE_SIZE];
	cli_args_t *args = (cli_args_t *)arg;
	int rx;
	while (does_box_exist(args->box_name)) { // while existing the box client, the thread will be  wanting for cli_pipe send a message to write
		rx = open(args->cli_pipename, O_RDONLY);
		if (rx == -1 ) {
			fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		// receber mensagem do pub e ecrever a box
        char buffer[BUFFER_SIZE], str[OP_CODE_SIZE + MESSAGE_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        memset(str, 0, OP_CODE_SIZE + MESSAGE_SIZE);
        ssize_t ret, red = 0;
        while (true) {
            ret = read(rx, buffer, BUFFER_SIZE - 1);
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
            fprintf(stdout, "sessions: Recebi %ld bytes\n", ret);
        }
        memcpy(msg, str+1, MESSAGE_SIZE);
        write_message(args->box_name, msg); // write message 
        fprintf(stdout, "mensagem: Mensagem total \"%d|%s\"\n", str[0], str + 1);
		if (close(rx) == -1) {
			fprintf(stderr, "[ERR]: close failed: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	unlink(args->cli_pipename);
	return 0;
}

void *man_thr_func(void* arg) {
	// nao faz nada, e so para compilar
	cli_args_t *args = (cli_args_t *)arg;
	memset(args->cli_pipename, 0, CLI_PIPENAME_SIZE);
	return 0;
}

int get_free_spot(pthreads_t ps) {
	int i;
	for (i = 0; i < ps.max_size; i++) {
		if (!ps.bools[i]) {return i;}
	}
	return -1;
}

pthreads_t pthreads_init(int max_sessions) {
	int i;
	size_t m_sessions = (size_t)max_sessions;
	pthreads_t ps;
	ps.len = 0;
	ps.pthread_arr = malloc(m_sessions * sizeof(pthread_t));
	ps.char_matrix = malloc(m_sessions);
	for (i = 0; i < max_sessions; i++) {ps.char_matrix[i] = malloc(CLI_PIPENAME_SIZE);}
	ps.bools = malloc(m_sessions * sizeof(bool));
	ps.max_size = max_sessions;
	return ps;
}

void pthreads_destroy(pthreads_t ps) {
	int i;
	free(ps.pthread_arr);
	for (i = 0; i < ps.max_size; i++) {free(ps.char_matrix[i]);}
	free(ps.char_matrix);
	free(ps.bools);
}

int pthreads_add(pthreads_t ps, char *cli_pipe, cli_type c_type, char* boxname) {  // for each session we have a thread
	if (ps.len == ps.max_size) {return -1;}
	int index = get_free_spot(ps);

	cli_args_t args = cli_args_init(cli_pipe, boxname);
	if (c_type == SUB) {
		if (pthread_create(&ps.pthread_arr[index], NULL, sub_thr_func, (void*)&args) != 0) {
		fprintf(stderr, "error creating thread.\n");
		return -1;
		}
	} else if (c_type == PUB) {
		if (pthread_create(&ps.pthread_arr[index], NULL, pub_thr_func, (void*)&args) != 0) {
		fprintf(stderr, "error creating thread.\n");
		return -1;
		}
	} else if (c_type == MAN) {
		if (pthread_create(&ps.pthread_arr[index], NULL, man_thr_func, (void*)&args) != 0) {
		fprintf(stderr, "error creating thread.\n");
		return -1;
		}
	}

	memcpy(ps.char_matrix[index], cli_pipe, CLI_PIPENAME_SIZE);
	ps.bools[index] = true;
	ps.len++;
	return 0;
}

int pthreads_remove(pthreads_t ps, char *cli_pipe) {
	int i = 0;
	for (i = 0; i < ps.max_size; i++) {
		if (memcmp(ps.char_matrix[i], cli_pipe, CLI_PIPENAME_SIZE) == 0) {
			if(pthread_join(ps.pthread_arr[i], NULL) != 0) {
				fprintf(stderr, "error joining thred.\n");
				return -1;
			}
			memset(ps.char_matrix[i], 0, CLI_PIPENAME_SIZE);
			ps.bools[i] = false;
			ps.len--;
			return 0;
		}
	}
	return -1;
}