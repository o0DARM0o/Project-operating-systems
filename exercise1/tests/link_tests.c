#include "fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

char const file_contents[] = "AAA!";
char const path1[] = "/f1";
char const path2[] = "/l2";
char const path3[] = "/l3";
char const p1[] = "/g1";
char const p2[] = "/p2";
char const p3[] = "/p3";

void assert_contents_ok(char const *path) {
    int f = tfs_open(path, 0);
    assert(f != -1);

    char buffer[sizeof(file_contents)];
	memset(buffer,0,sizeof(buffer));
    assert(tfs_read(f, buffer, sizeof(buffer)) == sizeof(buffer));
    assert(memcmp(buffer, file_contents, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);
}

int main() {
	assert(tfs_init(NULL) != -1);

	int fhandle;
	char read_contents[sizeof(file_contents)];
	assert((fhandle = tfs_open(path1, TFS_O_CREAT)) != -1);
	assert(tfs_write(fhandle, file_contents, sizeof(file_contents)) != -1);
	assert(tfs_close(fhandle) != -1);

	assert_contents_ok(path1);

	assert(tfs_sym_link(path1, path2) != -1);
	assert_contents_ok(path2);
	assert(tfs_sym_link(path2, path3) != -1);
	assert_contents_ok(path3);

	assert(tfs_unlink(path2) != -1);

	assert((fhandle = tfs_open(path1, TFS_O_CREAT)) != -1);
	assert(tfs_read(fhandle, read_contents, sizeof(read_contents)) != -1);
	assert(tfs_close(fhandle) != -1);

	assert(tfs_unlink(path1) != -1);
	assert(tfs_unlink(path3) != -1);

	assert((fhandle = tfs_open(path1, TFS_O_CREAT)) != -1);
	assert(tfs_read(fhandle, read_contents, sizeof(read_contents)) != -1);
	assert(tfs_close(fhandle) != -1);



	assert((fhandle = tfs_open(p1, TFS_O_CREAT)) != -1);
	assert(tfs_write(fhandle, file_contents, sizeof(file_contents)) != -1);
	assert(tfs_close(fhandle) != -1);

	assert_contents_ok(p1);

	assert(tfs_link(p1, p2) != -1);
	assert_contents_ok(p2);
	assert(tfs_link(p2, p3) != -1);
	assert_contents_ok(p3);

	assert(tfs_unlink(p2) != -1);

	assert((fhandle = tfs_open(p1, TFS_O_CREAT)) != -1);
	assert(tfs_read(fhandle, read_contents, sizeof(read_contents)) != -1);
	assert(tfs_close(fhandle) != -1);

	assert(tfs_unlink(p1) != -1);
	assert(tfs_unlink(p3) != -1);

	assert((fhandle = tfs_open(p1, TFS_O_CREAT)) != -1);
	assert(tfs_read(fhandle, read_contents, sizeof(read_contents)) != -1);
	assert(tfs_close(fhandle) != -1);

	printf("Successful test.\n");
    return 0;
}
