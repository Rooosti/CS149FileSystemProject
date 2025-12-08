/*
    Header file for custom file system implementation. It defines the data structures and
    function interfaces for a simple in-memory file system.
*/

#pragma once // Prevents multiple inclusions of the header.
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#define NAME_MAX 32 // Defines maximum length for file/directory names (31 characters + null terminator).
#define MAX_CHILDREN 64 // Defines maximum number of files/subdirectories per directory.

// File attribute flags (can be combined using bitwise OR).
// For example, if you wanted a hidden and read-only file, you would set node->attributes = ATTR_HIDDEN | ATTR_READONLY to get 0x03.
#define ATTR_NONE     0x00  // No special attributes.
#define ATTR_HIDDEN   0x01  // Hidden file/directory.
#define ATTR_READONLY 0x02  // Read-only file/directory.
#define ATTR_SYSTEM   0x04  // System file/directory.
#define ATTR_ARCHIVE  0x08  // Archive bit (modified since last backup).

// Defines two types of file system nodes: N_DIR & N_FILE.
// N_DIR: directory (can contain other files/directories).
// N_FILE: regular file (contains data).
typedef enum { N_DIR=1, N_FILE=2 } node_type;

// Core data structure:
typedef struct node {
    node_type type; // N_DIR or N_FILE.
    char name[NAME_MAX+1]; // File/directory name.
    struct node *parent; // Pointer to parent directory.

    // Metadata:
    time_t created;    // Creation timestamp.
    time_t modified;   // Last modification timestamp.
    time_t accessed;   // Last access timestamp.
    uint8_t attributes; // File attributes (ATTR_* flags).

    // For directories:
    struct node *children[MAX_CHILDREN]; // Array of child nodes.
    size_t child_count; // Number of children.

    // For files:
    uint8_t *data; // File content (dynamically allocated).
    size_t size; // Current file size.
    size_t cap; // Allocated capacity.
} node_t;

// File system operations:
// System management:
void fs_init(void); // Initialize the file system.
void fs_destroy(void); // Clean up and free memory.
int fs_cd(const char *path); // Change current working directory of file system (supports relative paths and navigation).

// Directory operations:
int mkdir_p(const char *path); // Create directory (parents optional).
int rmdir_empty(const char *path); // Remove empty directory.
int ls_dir(const char *path); // List directory contents.
int fs_search(const char *term); // Search file system for a file name that matches with term

// File operations:
int create_file(const char *path); // Create empty file.
ssize_t write_file(const char *path, size_t off, const void *buf, size_t len); // Write to file.
ssize_t read_file(const char *path, size_t off, void *buf, size_t len); // Read from file.
int rm_file(const char *path); // Remove file.
int rename_file(const char *old_path, const char *new_path); // Rename/move file or directory.

// File descriptor operations:
#define MAX_OPEN_FILES 32 // Maximum number of simultaneously open files.
#define O_RDONLY 0x01 // Read-only mode.
#define O_WRONLY 0x02 // Write-only mode.
#define O_RDWR   0x03 // Read-write mode.

int fs_open(const char *path, int flags); // Open file and return file descriptor.
int fs_close(int fd); // Close file descriptor.
ssize_t fs_read_fd(int fd, void *buf, size_t len); // Read from file descriptor.
ssize_t fs_write_fd(int fd, const void *buf, size_t len); // Write to file descriptor.
off_t fs_seek(int fd, off_t offset, int whence); // Seek to position in file.

// Metadata operations:
// Use new data structure to return just metadata information without other node information.
typedef struct file_info {
    node_type type;      // File or directory.
    char name[NAME_MAX+1]; // File name.
    time_t created;      // Creation time.
    time_t modified;     // Last modification time.
    time_t accessed;     // Last access time.
    uint8_t attributes;  // File attributes.
    size_t size;         // File size (0 for directories).
    size_t child_count;  // Number of children (for directories).
} file_info_t;

// Retrieve complete metadata for a file or directory.
int get_file_info(const char *path, file_info_t *info); 

// Set file attributes.
int set_file_attributes(const char *path, uint8_t attributes); 

// Used when file is either accessed or modified.
int touch_file(const char *path); 

// Helper function to display timestamp.
const char* format_time(time_t timestamp); 

// Helper function display attributes.
const char* format_attributes(uint8_t attributes); 
