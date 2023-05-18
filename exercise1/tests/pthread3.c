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
char path2[] = "/l1";
char path3[] = "/l2";

void assert_contents_ok(char const *path) {
    int f = tfs_open(path, 0);
    assert(f != -1);

    char buffer[sizeof(file_contents)];
	memset(buffer,0,sizeof(buffer));
    assert(tfs_read(f, buffer, sizeof(buffer)) == sizeof(buffer));
    assert(memcmp(buffer, file_contents, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);
}

void *doSomething() {
    int fhandle, var;
    char read_contents[sizeof(file_contents)];
    
    assert((fhandle = tfs_open(path1, TFS_O_CREAT)) != -1);
	assert(tfs_write(fhandle, file_contents, sizeof(file_contents)) != -1);
	assert(tfs_close(fhandle) != -1);

    assert_contents_ok(path1);

    assert(var = tfs_sym_link(path1, path2) != -1);
    assert_contents_ok(path2);

    assert(var = tfs_sym_link(path2, path3) != -1);
    assert_contents_ok(path3);

    assert(var = tfs_unlink(path2) != -1);

	assert((fhandle = tfs_open(path1, TFS_O_CREAT)) != -1);
	assert(tfs_read(fhandle, read_contents, sizeof(read_contents)) != -1);
	assert(tfs_close(fhandle) != -1);

    assert(var = tfs_unlink(path1) != -1);

    assert(var = tfs_unlink(path3) != -1);

    assert((fhandle = tfs_open(path1, TFS_O_CREAT)) != -1);
	assert(tfs_read(fhandle, read_contents, sizeof(read_contents)) != -1);
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