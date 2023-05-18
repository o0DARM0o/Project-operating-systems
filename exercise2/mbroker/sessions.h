#include "logging.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "../fs/operations.h"
#include "boxes.h"

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


typedef enum {SUB, PUB, MAN} cli_type;

typedef struct {
	char cli_pipename[256];
	char box_name[32];
} cli_args_t;

typedef struct {
	pthread_t *pthread_arr;
	char **char_matrix;
	bool *bools;
	int len;
	int max_size;
} pthreads_t;

cli_args_t cli_args_init(char *cli_pipe, char *b_name);

void *sub_thr_func(void* arg);
void *pub_thr_func(void* arg);
void *man_thr_func(void* arg);

int get_free_spot(pthreads_t ps);

pthreads_t pthreads_init(int max_sessions);

void pthreads_destroy(pthreads_t ps);

int pthreads_add(pthreads_t ps, char *cli_pipe, cli_type c_type, char* boxname);

int pthreads_remove(pthreads_t ps, char *cli_pipe);