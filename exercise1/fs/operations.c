/* Projeto realizado por:
    102760 - Diogo Marques
    103215 - Leonardo Pinheiro
*/

#include "operations.h"
#include "config.h"
#include "state.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "betterassert.h"

tfs_params tfs_default_params() {
    tfs_params params = {
        .max_inode_count = 64,
        .max_block_count = 1024,
        .max_open_files_count = 16,
        .block_size = 1024,
    };
    return params;
}

int tfs_init(tfs_params const *params_ptr) {
    tfs_params params;
    if (params_ptr != NULL) {
        params = *params_ptr;
    } else {
        params = tfs_default_params();
    }

    if (state_init(params) != 0) {
        return -1;
    }

    // create root inode
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    if (state_destroy() != 0) {
        return -1;
    }
    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}

/**
 * Looks for a file.
 *
 * Note: as a simplification, only a plain directory space (root directory only)
 * is supported.
 *
 * Input:
 *   - name: absolute path name
 *   - root_inode: the root directory inode
 * Returns the inumber of the file, -1 if unsuccessful.
 */
static int tfs_lookup(char const *name, inode_t *root_inode) {
    // TODO: assert that root_inode is the root directory
    if (!valid_pathname(name)) {
        printf("Este if\n");
        return -1;
    }

    // skip the initial '/' character
    name++;
    return find_in_dir(root_inode, name);
}

int tfs_open(char const *name, tfs_file_mode_t mode) {
    // Checks if the path name is valid
    if (!valid_pathname(name)) {return -1;}
    char *pathname = malloc(strlen(name));
    strcpy(pathname, name);
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_open: root dir inode must exist");
    int inum = tfs_lookup(pathname, root_dir_inode);
    size_t offset;
    
    if (inum >= 0) {
        // The file already exists
        inode_t *inode = inode_get(inum);
        ALWAYS_ASSERT(inode != NULL,
                      "tfs_open: directory files must have an inode");
        pthread_rwlock_wrlock(&inode->lock);
        // Checks if it's a softlink
        if (inode->is_softlink) {
            void *block;
            if ((block = data_block_get(inode->i_data_block)) == NULL) {
                free(pathname);
                pthread_rwlock_unlock(&inode->lock);
                return -1;
            }
            pathname =realloc(pathname, strlen(block));
            strcpy(pathname, block);
            pthread_rwlock_unlock(&inode->lock);
            if ((inum = tfs_lookup(pathname, root_dir_inode)) == -1) {
                free(pathname);
                return -1;
            }
            if ((inode = inode_get(inum)) == NULL) {
                free(pathname);
                return -1;
            }
            pthread_rwlock_wrlock(&inode->lock);
        }
        // Truncate (if requested)
        if (mode & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                data_block_free(inode->i_data_block);
                inode->i_size = 0;
            }
        }
        // Determine initial offset
        if (mode & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
        pthread_rwlock_unlock(&inode->lock);
    } else if (mode & TFS_O_CREAT) {
        // The file does not exist; the mode specified that it should be created
        // Create inode
        inum = inode_create(T_FILE);
        
        if (inum == -1) {
            free(pathname);
            return -1; // no space in inode table
        }

        // Add entry in the root directory
        if (add_dir_entry(root_dir_inode, pathname + 1, inum) == -1) {
            inode_delete(inum);
            free(pathname);
            return -1; // no space in directory
        }

        offset = 0;
    } else {
        free(pathname);
        return -1;
    }
    free(pathname);
    // Finally, add entry to the open file table and return the corresponding
    // handle 
    return add_to_open_file_table(inum, offset);

    // Note: for simplification, if file was created with TFS_O_CREAT and there
    // is an error adding an entry to the open file table, the file is not
    // opened but it remains created
}

int tfs_sym_link(char const *targetname, char const *link_name) {
    // Test pathnames
    if (!valid_pathname(targetname)) {return -1;}
    if (!valid_pathname(link_name)) {return -1;}
    // Test root_dir_inode
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    if (root_dir_inode == NULL) {return -1;}
    int inum = tfs_lookup(targetname, root_dir_inode);
    if (inum == -1) {return -1;}
    inode_t *inode = inode_get(inum);
    if (inode == NULL) {return -1;}
    void *block;
    char *target = malloc(strlen(targetname));
    pthread_rwlock_rdlock(&inode->lock);
    if (inode->is_softlink) {
        block = data_block_get(inode->i_data_block);
        if (block == NULL || !valid_pathname(block)) {
            pthread_rwlock_unlock(&inode->lock);
            free(target);
            return -1;
        }
        pthread_rwlock_unlock(&inode->lock);
        target = realloc(target, strlen(block));
        strcpy(target, block);
        inum = tfs_lookup(target, root_dir_inode);
        if (inum == -1) {
            free(target);
            return -1;
        }
        inode = inode_get(inum);
        if (inode == NULL) {
            free(target);
            return -1;
        }
    } else {
        pthread_rwlock_unlock(&inode->lock);
        strcpy(target, targetname);
    }
    // Test length of target's pathname
    size_t to_write = strlen(target);
    if (to_write > state_block_size()) {
        free(target);
        return -1;
    }
    // Create an inode for the softlink
    inum = inode_create(T_SOFTLINK);
    inode = inode_get(inum);
    // allocate a data_block to softlink's inode
    int bnum = data_block_alloc();
    if (bnum == -1) {
        inode_delete(inum);
        free(target);
        return -1;
    }
    pthread_rwlock_wrlock(&inode->lock);
    inode->i_data_block = bnum;
    block = data_block_get(inode->i_data_block);
    if (block == NULL) {
        pthread_rwlock_unlock(&inode->lock);
        inode_delete(inum);
        free(target);
        return -1;
    }
    // write target's pathname into that block
    memcpy(block, target, to_write);
    // add softlink as a dir_entry
    if (add_dir_entry(root_dir_inode, link_name + 1, inum) == -1) {
        pthread_rwlock_unlock(&inode->lock);
        inode_delete(inum);
        free(target);
        return -1;
    }
    // update size
    inode->i_size += to_write; 
    pthread_rwlock_unlock(&inode->lock);
    free(target);
    return 0;
}

int tfs_link(char const *target, char const *link_name) {
    if (!valid_pathname(target)) {return -1;}
    if (!valid_pathname(link_name)) {return -1;}

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    if (root_dir_inode == NULL) {return -1;}

    int inum = tfs_lookup(target, root_dir_inode);
    if (inum == -1) {return -1;}

    inode_t *inode = inode_get(inum);
    if (inode == NULL) {return -1;}

    pthread_rwlock_wrlock(&inode->lock);

    if(inode->is_softlink) {
        pthread_rwlock_unlock(&inode->lock);
        return -1;
    }

    if (add_dir_entry(root_dir_inode, link_name + 1, inum) == -1) {
        pthread_rwlock_unlock(&inode->lock);
        return -1;
    }

    inode->cnt_hardlinks++;
    pthread_rwlock_unlock(&inode->lock);
    return 0;
}

int tfs_close(int fhandle) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1; // invalid fd
    }
    remove_from_open_file_table(fhandle);

    return 0;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    //  From the open file table entry, we get the inode
    inode_t *inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_write: inode of open file deleted");

    // Determine how many bytes to write
    size_t block_size = state_block_size();
    pthread_rwlock_rdlock(&file->lock);
    if (to_write + file->of_offset > block_size) {
        to_write = block_size - file->of_offset;
    }
    if (to_write > 0) {
        pthread_rwlock_wrlock(&inode->lock);
        if (inode->i_size == 0) {

            // If empty file, allocate new block
            int bnum = data_block_alloc();
            if (bnum == -1) {
                pthread_rwlock_unlock(&inode->lock);
                pthread_rwlock_unlock(&file->lock);
                return -1; // no space
            }
            inode->i_data_block = bnum;
        }
        void *block = data_block_get(inode->i_data_block);
        pthread_rwlock_unlock(&inode->lock);
        pthread_rwlock_unlock(&file->lock);
        ALWAYS_ASSERT(block != NULL, "tfs_write: data block deleted mid-write");
        pthread_rwlock_wrlock(&file->lock);
        pthread_rwlock_wrlock(&inode->lock);

        // Perform the actual write
        memcpy(block + file->of_offset, buffer, to_write);

        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_write;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
        pthread_rwlock_unlock(&inode->lock);
    }
    pthread_rwlock_unlock(&file->lock);
    return (ssize_t)to_write;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }
    pthread_rwlock_wrlock(&file->lock);
    // From the open file table entry, we get the inode
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        pthread_rwlock_unlock(&file->lock);
        ALWAYS_ASSERT(false, "tfs_read: inode of open file deleted");
    }
    pthread_rwlock_rdlock(&inode->lock);
    // Determine how many bytes to read
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }
    if (to_read > 0) {
        void *block = data_block_get(inode->i_data_block);
        if (block == NULL) {
            pthread_rwlock_unlock(&inode->lock);
            pthread_rwlock_unlock(&file->lock);
            ALWAYS_ASSERT(false, "tfs_read: data block deleted mid-read");
        }

        // Perform the actual read
        memcpy(buffer, block + file->of_offset, to_read);
        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_read;
    }
    pthread_rwlock_unlock(&inode->lock);
    pthread_rwlock_unlock(&file->lock);
    return (ssize_t)to_read;
}

int tfs_unlink(char const *target) {
    if (!valid_pathname(target)) {return -1;}

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    if (root_dir_inode == NULL) {return -1;}

    int inum = tfs_lookup(target, root_dir_inode);
    if (inum == -1) {return -1;}

    inode_t *inode = inode_get(inum);
    if (inode == NULL) {return -1;}

    pthread_rwlock_wrlock(&inode->lock);
    if (clear_dir_entry(root_dir_inode, target + 1) == -1) {return -1;}
    inode->cnt_hardlinks--;
    
    if (inode->cnt_hardlinks == 0) {
        pthread_rwlock_unlock(&inode->lock);
        inode_delete(inum);
        return 0;
    }
    pthread_rwlock_unlock(&inode->lock);
    return 0;
}

int tfs_copy_from_external_fs(char const *source_path, char const *dest_path) {
    int fdCopy;
    FILE *fd;
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    fd = fopen(source_path, "r");
    if (fd == NULL) {return -1;}
    fdCopy = tfs_open(dest_path, TFS_O_CREAT | TFS_O_TRUNC | TFS_O_APPEND);
    if (fdCopy == -1) {
        fclose(fd);
        tfs_close(fdCopy);
        return -1;
    }
    while (fread(&buffer, sizeof(buffer), 1, fd) > 0) {
        if ((int) tfs_write(fdCopy, &buffer, sizeof(buffer)) < 0) {
            fclose(fd);
            tfs_close(fdCopy);
            return -1;
        }
        memset(buffer, 0, sizeof(buffer));
    }
    if ((int) tfs_write(fdCopy, &buffer, strlen(buffer)) < 0) {
            fclose(fd);
            tfs_close(fdCopy);
            return -1;
        }
    fclose(fd);
    tfs_close(fdCopy);
    return 0;
}