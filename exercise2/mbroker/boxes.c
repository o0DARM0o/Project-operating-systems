
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../fs/operations.h"

#include "../msg.h"
#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>

bool does_box_exist(char *box_name){
    char str[strlen(box_name)+1];
    strcpy(str, "/");
    strcat(str, box_name);    // add "/" to the box name
    int fhandle = tfs_open(str, TFS_O_APPEND); // if the tfs_open can open the box, means that exists
    tfs_close(fhandle);
    return fhandle != -1;
}

int create_box(char *box_name){

    char str[strlen(box_name)+1];
    strcpy(str, "/");
    strcat(str, box_name);   // add "/" to the box name
    if(does_box_exist(box_name)){
        return -1;
    }
    int fhandle = tfs_open(str, TFS_O_CREAT);
    if(fhandle != -1){
        tfs_close(fhandle);
        return 0;
    }
    return -1;
}

int remove_box(char *box_name){

    char str[strlen(box_name)+1];
    strcpy(str, "/");
    strcat(str, box_name);   // add "/" to the box name

    int fhandle = tfs_open(str, TFS_O_APPEND);
    if(fhandle == -1){
        return -1;
    }
    tfs_close(fhandle);
    tfs_unlink(str);
    return 0;
    

}

int write_message(char *box_name, char *message){
    printf("aqui: %s\n\n", message);
    char str[strlen(box_name)+1];
    strcpy(str, "/");
    strcat(str, box_name);

    int fhandle = tfs_open(str, TFS_O_APPEND);
    if(fhandle == -1){
        return -1;
    }
    size_t succeed = (size_t)tfs_write(fhandle, message, strlen(message));
    if(succeed == -1){
        tfs_close(fhandle);
        return -1;
    }
    tfs_close(fhandle);
    fhandle = tfs_open(str, 0);
    char buffer2[MESSAGE_SIZE];
    tfs_read(fhandle, buffer2, strlen(message));
    printf("write = %s\n",buffer2);
    tfs_close(fhandle);
    return 0;
}

int read_box(char * dest, char *box_name){

    char str[strlen(box_name)+1];
    strcpy(str, "/");
    strcat(str, box_name);

    int fhandle = tfs_open(str, 0);
    if(fhandle == -1){
        return -1;
    }
    size_t succeed = (size_t)tfs_read(fhandle, dest, MESSAGE_SIZE);
    if(succeed == -1){
        tfs_close(fhandle);
        return -1;
    }
    return 0;
}