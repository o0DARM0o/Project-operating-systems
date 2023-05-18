#include "operations.h"
#include "config.h"
#include "state.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "betterassert.h"

#define NUM_PTHREADS 100

char const file_contents[] = "AAA!";
char arr[] = "1234";
char path1[] = "/f1";

void *doSomething() {
    int fhandle, var;
    assert((fhandle = tfs_open(path1, TFS_O_CREAT)) != -1);
    assert((var = (int)tfs_write(fhandle, file_contents, strlen(file_contents))) != -1);
    assert(tfs_close(fhandle) != -1);
    assert((fhandle = tfs_open(path1, 0)) != -1);
    assert((var = (int)tfs_read(fhandle, arr, strlen(file_contents))) != -1);
    assert(tfs_close(fhandle) != -1);
    assert(strcmp(file_contents, arr) == 0);

    return NULL;
}

int main() {
    int  i = 0;
    pthread_t tid[NUM_PTHREADS];

    assert(tfs_init(NULL) != -1);
    while (i < NUM_PTHREADS) {
        if (pthread_create(&tid[i], NULL, doSomething, NULL) != 0) {
            fprintf(stderr, "failed to create thread: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        i++;
    }
    for (i = 0; i < NUM_PTHREADS; i++) {pthread_join(tid[i], NULL);}
    assert(tfs_destroy() != -1);

    printf("Successful test.\n");
    return 0;
}