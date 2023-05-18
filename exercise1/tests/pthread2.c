#include "operations.h"
#include "config.h"
#include "state.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "betterassert.h"

#define NUM_PTHREADS 100

char path1[] = "/f1";
char path2[] = "/l1";
char path3[] = "/l2";
const char write_content[] = "BBB! BBB! BBB! BBB!";

void assert_contents_ok(char const *path) {
    int f = tfs_open(path, 0);
    assert(f != -1);

    char buffer[sizeof(write_content)];
    assert(tfs_read(f, buffer,sizeof(write_content)));
    assert(memcmp(buffer, write_content, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);
}

void *doSomething() {
    int fhandle, var;
    char read_contents[sizeof(write_content)];
    assert((fhandle = tfs_open(path1, TFS_O_CREAT)) != -1);
    assert((var = (int)tfs_write(fhandle, write_content, sizeof(write_content))) != -1);
    assert(tfs_close(fhandle) != -1);
    
    assert_contents_ok(path1);

    assert(var = tfs_link(path1, path2) != -1);
    assert_contents_ok(path2);
    assert(var = tfs_link(path2, path3) != -1);
    assert_contents_ok(path3);

    assert(var = tfs_unlink(path2) != -1);

    fhandle = tfs_open(path1, TFS_O_CREAT);
    assert(fhandle != -1);

    assert(tfs_read(fhandle, read_contents, sizeof(read_contents))  != -1);
    assert(tfs_close(fhandle) != -1);

    assert(var = tfs_unlink(path1) != -1);

    fhandle = tfs_open(path3, TFS_O_CREAT);
    assert(fhandle != -1);

    assert(tfs_read(fhandle, read_contents, sizeof(read_contents))  != -1);
    assert(tfs_close(fhandle) != -1);
      
    assert(var = tfs_unlink(path3) != -1);

    fhandle = tfs_open(path1, TFS_O_CREAT);
    assert(fhandle != -1);

    assert(tfs_read(fhandle, read_contents, sizeof(read_contents)) == 0);
    assert(tfs_close(fhandle) != -1);



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