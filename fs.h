#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define NAME_MAX 32
#define MAX_CHILDREN 64

typedef enum { N_DIR=1, N_FILE=2 } node_type;

typedef struct node {
    node_type type;
    char name[NAME_MAX+1];
    struct node *parent;

    // For directories
    struct node *children[MAX_CHILDREN];
    size_t child_count;

    // For files
    uint8_t *data;
    size_t size;
    size_t cap;
} node_t;

void fs_init(void);
void fs_destroy(void);

int mkdir_p(const char *path); // create directory (parents optional)
int create_file(const char *path); // create empty file
ssize_t write_file(const char *path, size_t off, const void *buf, size_t len);
ssize_t read_file(const char *path, size_t off, void *buf, size_t len);
int rm_file(const char *path);
int rmdir_empty(const char *path);
int ls_dir(const char *path); // print to stdout for simplicity
