#include "logging.h"
#include "producer-consumer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int pcq_create(pc_queue_t *queue, size_t capacity){
    queue->pcq_capacity = capacity;
    queue->pcq_current_size  = 0;

    return 0;

}