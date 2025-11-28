/*
    Implementation file for the custom file system. Contains actual code to make the file system work.
*/
#include "fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global root pointer that points to the root directory of the file system tree.
static node_t *root;
static node_t *cwd;

// Node creation:
static node_t *node_new(node_type t, const char *name, node_t *parent) {

    // Set up basic properties of a node (type, name, and parent).
    node_t *n = calloc(1, sizeof(*n)); // Allocate memory and set to zero.
    n->type = t; n->parent = parent;
    strncpy(n->name, name, NAME_MAX);
    
    // Initialize metadata timestamps.
    time_t now = time(NULL);
    n->created = now;
    n->modified = now;
    n->accessed = now;
    n->attributes = ATTR_NONE; // Start with no special attributes.
    
    return n;
}

// Initialize the file system by creating the root directory.
// Also set current working directory to root.
void fs_init(void) {
    root = node_new(N_DIR, "", NULL); // Root has empty name and no parent.
    cwd = root;
}

// Node deletion/clean-up:
// Recursively frees entire tree starting from any given node.
static void node_free(node_t *n) {
    
    // Validate input.
    if (!n) return;

    // If directory node, recursively free all children first.
    if (n->type == N_DIR)
        for (size_t i=0;i<n->child_count;i++) node_free(n->children[i]);

    // For files, free the data buffer.
    free(n->data);
    free(n);
}

// Clean up the entire file system.
void fs_destroy(void) { node_free(root); root = NULL; }

// Directory management helper functions:
// Add a child to a directory.
static node_t *dir_add(node_t *dir, node_t *child) {

    // Validate that passed in node is a directory and is not full.
    if (!dir || dir->type!=N_DIR || dir->child_count>=MAX_CHILDREN) return NULL;

    dir->children[dir->child_count++] = child;
    return child;
}

// Find a child by name in directory.
static node_t *dir_find(node_t *dir, const char *name) {

    // Validate that passed in node is a directory.
    if (!dir || dir->type!=N_DIR) return NULL;

    // Use linear search through children array.
    for (size_t i=0;i<dir->child_count;i++) {
        if (strncmp(dir->children[i]->name, name, NAME_MAX)==0) {
            return dir->children[i];
        } 
    }

    return NULL;
}


static node_t *walk_from(node_t *start,
                         const char *path,
                         int want_parent,
                         char out_leaf[NAME_MAX+1]) {
    if (!path) return NULL;

    int absolute = (path[0] == '/');
    node_t *cur = absolute ? root : (start ? start : root);

    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp));
    tmp[sizeof(tmp)-1] = 0;

    char *save = NULL;
    char *tok  = strtok_r(tmp, "/", &save);
    char *next = strtok_r(NULL, "/", &save);

    // Case: path is just "/" or ""
    if (!tok) {
        return want_parent ? cur : cur;
    }

    while (tok) {
        if (strlen(tok) == 0 || strlen(tok) > NAME_MAX) return NULL;

        if (strcmp(tok, ".") == 0) {
            // stay in cur
        } else if (strcmp(tok, "..") == 0) {
            if (cur->parent) cur = cur->parent; // root stays at root
        } else if (!next) {
            // last component
            if (want_parent) {
                if (out_leaf) {
                    strncpy(out_leaf, tok, NAME_MAX);
                    out_leaf[NAME_MAX] = '\0';
                }
                return cur;
            }
            return dir_find(cur, tok);
        } else {
            // middle component: must be a directory we can descend into
            cur = dir_find(cur, tok);
            if (!cur || cur->type != N_DIR) return NULL;
        }

        tok  = next;
        next = strtok_r(NULL, "/", &save);
    }

    // if we consumed everything cleanly and there was no special last component
    return want_parent ? cur : cur;
}

int fs_cd(const char *path) {
    node_t *d = walk_from(cwd, path, 0, NULL);
    if (!d || d->type != N_DIR) return -1;
    cwd = d;
    return 0;
}

static node_t *walk(const char *path, int want_parent, char out_leaf[NAME_MAX+1]) {
    return walk_from(cwd, path, want_parent, out_leaf);
}

int mkdir_p(const char *path) {
    if (!path) return -1;
    if (strcmp(path, "/") == 0 || strcmp(path, "") == 0) return 0;

    int absolute = (path[0] == '/');
    node_t *cur  = absolute ? root : cwd;

    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp));
    tmp[sizeof(tmp)-1] = 0;

    char *save = NULL;
    char *tok  = strtok_r(tmp, "/", &save);

    // Case: just "/" or "//"
    if (!tok) return 0;

    while (tok) {
        if (strlen(tok) == 0 || strlen(tok) > NAME_MAX) return -1;

        if (strcmp(tok, ".") == 0) {
            // stay
        } else if (strcmp(tok, "..") == 0) {
            if (cur->parent) cur = cur->parent;
        } else {
            // normal directory name
            node_t *n = dir_find(cur, tok);
            if (!n) {
                n = node_new(N_DIR, tok, cur);
                if (!dir_add(cur, n)) return -1;
            } else if (n->type != N_DIR) {
                // trying to mkdir where a file already exists
                return -1;
            }
            cur = n;
        }

        tok = strtok_r(NULL, "/", &save);
    }

    return 0;
}

// Implements empty file creation in file system.
int create_file(const char *path) {

    // Parse path for file creation using leaf buffer (stores the filename, which is the last component of the path).
    char leaf[NAME_MAX+1] = {0};

    // Use want_parent = 1 to get the parent directory. 
    // For example, for "/documents/myfile.txt", returns "documents/" and puts "myfile.txt" in leaf.
    node_t *parent = walk(path, 1, leaf);

    // Validate that the parent exists and it is a directory node.
    if (!parent || parent->type!=N_DIR) return -1;

    // Prevent duplicate file creation so that we don't overwrite.
    if (dir_find(parent, leaf)) return -1;

    // Create file node.
    node_t *f = node_new(N_FILE, leaf, parent);

    // Return result.
    return dir_add(parent, f) ? 0 : -1;
}

// Implements dynamic memory management to handle growing file storage as per needs.
static int ensure_cap(node_t *f, size_t want) {

    // Check if we need to grow file size (early exits if we already have enough capacity).
    if (f->cap >= want) return 0;

    // Calculate new capacity otherwise:
    size_t newcap = f->cap ? f->cap : 64; // Start with initial capacity of 64 bytes if file has no capacity yet.
    while (newcap < want) newcap *= 2; // Double space until we have sufficient storage capacity.
    uint8_t *p = realloc(f->data, newcap); // Reallocate memory using realloc() while preserving existing data by copying old content to new location if necessary. This also frees old memory.
    if (!p) return -1; // Error handling if allocation fails (out of memory).

    // Zero new memory region:
    // Initialize new bytes to zero.
    // p + f->cap: points to the start of the new memory region.
    // newcap - f->cap: the size of the new region to zero.
    if (newcap > f->cap) memset(p + f->cap, 0, newcap - f->cap);

    // Update file structure:
    f->data = p; // Update data pointer to new memory location.
    f->cap = newcap; // Update capacity to reflect new size.
    return 0; 
}

// Implementats file write operation at a specific offset.
// path: file path.
// off: byte offset indicating where to start writing.
// buf: pointer to data to write.
// len: number of bytes to write.
ssize_t write_file(const char *path, size_t off, const void *buf, size_t len) {

    // Find the file to write to using walk() & want_parent = 0, which will return actual file node.
    node_t *f = walk(path, 0, NULL);

    // Validate that the file exists and is not a directory.
    if (!f || f->type!=N_FILE) return -1;

    // Calculate the total space needed for write operation.
    size_t need = off + len; // Need is the total file size required AFTER the write.

    // Ensure there's sufficient capacity for this operation by calling ensure_cap().
    if (ensure_cap(f, need) < 0) return -1;

    // f->data + off: points to the write location in the file's buffer.
    // Copy len bytes from buf to file write location.
    memcpy(f->data + off, buf, len);

    // Update the file size if write extended file.
    if (need > f->size) f->size = need;

    // Update metadata: file was modified and accessed.
    time_t now = time(NULL);
    f->modified = now;
    f->accessed = now;

    // Return success.
    return (ssize_t)len;
}

// Implements file read operation starting from a specific offset.
// Same parameters as write_file().
ssize_t read_file(const char *path, size_t off, void *buf, size_t len) {

    // Find the file to read from using walk() and want_parent = 0 to find the file node.
    node_t *f = walk(path, 0, NULL);

    // Validate that the file exists and is not a directory.
    if (!f || f->type!=N_FILE) return -1;

    // Check for end-of-file (offset is at or beyond file size).
    if (off >= f->size) return 0; // If EOF detected, return 0 to indicate no bytes were read.

    // Calculate actual read size in bytes.
    size_t n = f->size - off; // n = f->size - off: bytes available from offset to EOF.
    if (n > len) n = len; // Don't read more than requested.

    // Copy data to buffer.
    // f->data + off points to the read location in the file.
    // Copy n bytes from file to the provided buffer (handles any data type).
    memcpy(buf, f->data + off, n);

    // Update metadata: file was accessed.
    f->accessed = time(NULL);

    // Return bytes read, similar to UNIX read().
    return (ssize_t)n;
}

// Implements file deletion/removal from file system.
int rm_file(const char *path) {

    // Parse path to get parent directory.
    char leaf[NAME_MAX+1]={0}; // Stores filename to be deleted.
    node_t *parent = walk(path, 1, leaf); // Use walk() & want_parent = 1 to get the containing directory.
    if (!parent) return -1; 

    // Search for the file in the parent directory using linear search on the parent directory's children array.
    for (size_t i=0;i<parent->child_count;i++) {
        node_t *c = parent->children[i];

        // Name must match filename (leaf) and the node type must be a file, not a directory.
        if (strncmp(c->name, leaf, NAME_MAX)==0 && c->type==N_FILE) {
            
            // Perform removal using swap-with-last algorithm.
            // This removes in O(1), doesn't shift other files, and only requires a copy operation.
            // However, it does not preserve file position/order in the directory listing.
            node_free(c);
            parent->children[i] = parent->children[parent->child_count-1];
            parent->child_count--;
            return 0;
        }
    }
    return -1;
}

// Implements empty directory removal for file system.
int rmdir_empty(const char *path) {

    // Find the directory to remove using walk() & want_parent = 0, which returns the actual directory node.
    node_t *d = walk(path, 0, NULL);

    // Safety validation: directory must exist, must be directory type, and cannot be root directory.
    if (!d || d->type!=N_DIR || d==root) return -1;

    // Check if the directory is empty (only empty directories can be removed, similar to UNIX rmdir).
    if (d->child_count) return -1;

    // Get parent directory of directory to be removed.
    node_t *p = d->parent;

    // Find and remove directory using swap-with-last algorithm (linear search over children).
    for (size_t i = 0; i < p->child_count; i++) {
        
        // Use pointer comparison instead of name comparison.
        if (p->children[i] == d) { 
            node_free(d);
            p->children[i] = p->children[p->child_count-1];
            p->child_count--;
            return 0;
        }
    }

    return -1;
}

// Implements directory content listing, similar to UNIX ls.
int ls_dir(const char *path) {
    node_t *d = NULL;

    if (path == NULL || path[0] == '\0' || strcmp(path, ".") == 0) {
        d = cwd;
    } else if (strcmp(path, "/") == 0) {
        d = root;
    } else {
        d = walk(path, 0, NULL);  // uses cwd inside if you've wrapped it
    }

    if (!d || d->type != N_DIR) return -1;

    // Update metadata: directory was accessed.
    d->accessed = time(NULL);

    for (size_t i = 0; i < d->child_count; i++) {
        node_t *c = d->children[i];
        printf("%s%s\n", c->name, c->type == N_DIR ? "/" : "");
    }
    return 0;
}

// Metadata operations implementation:

// Get comprehensive file/directory information.
int get_file_info(const char *path, file_info_t *info) {
    if (!info) return -1;
    
    // Find the file or directory.
    node_t *n = walk(path, 0, NULL);
    if (!n) return -1;
    
    // Fill the info structure (see header file for structure details).
    info->type = n->type;
    strncpy(info->name, n->name, NAME_MAX);
    info->name[NAME_MAX] = '\0';
    info->created = n->created;
    info->modified = n->modified;
    info->accessed = n->accessed;
    info->attributes = n->attributes;
    
    // If file node, we need to retrieve the size and there are no children.
    if (n->type == N_FILE) {
        info->size = n->size;
        info->child_count = 0;
    // If directory node, there is no size and there are children.
    } else {
        info->size = 0;
        info->child_count = n->child_count;
    }
    
    // Update access time since we accessed the node.
    n->accessed = time(NULL);
    
    return 0;
}

// Set file/directory attributes. 
// Can set multiple using bitwise | (OR) operator.
int set_file_attributes(const char *path, uint8_t attributes) {
    
    // Find the file using walk() & want_parent = 0, which returns the final component/file to be set.
    node_t *n = walk(path, 0, NULL);
    if (!n) return -1;
    
    // Update attributes.
    n->attributes = attributes;
    n->modified = time(NULL); // Changing attributes counts as modification.
    
    return 0;
}

// Update access and modification times (like UNIX touch command).
int touch_file(const char *path) {

    // Find the file using walk() & want_parent = 0, which returns the final component/file to be set.
    node_t *n = walk(path, 0, NULL);
    if (!n) return -1;
    
    // Need to update both the access time and modification time.
    time_t now = time(NULL);
    n->accessed = now;
    n->modified = now;
    
    return 0;
}

// Format timestamp for human-readable display.
const char* format_time(time_t timestamp) {
    static char buffer[32];
    struct tm *tm_info = localtime(&timestamp);
    
    if (!tm_info) {
        strncpy(buffer, "Invalid time", sizeof(buffer));
        return buffer;
    }
    
    // Format: "YYYY-MM-DD HH:MM:SS".
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

// Format attributes for human-readable display.
const char* format_attributes(uint8_t attributes) {
    static char buffer[32];
    buffer[0] = '\0';
    
    if (attributes == ATTR_NONE) {
        strncpy(buffer, "none", sizeof(buffer));
        return buffer;
    }
    
    int first = 1;
    if (attributes & ATTR_HIDDEN) {
        strncat(buffer, first ? "hidden" : ",hidden", sizeof(buffer) - strlen(buffer) - 1);
        first = 0;
    }
    if (attributes & ATTR_READONLY) {
        strncat(buffer, first ? "readonly" : ",readonly", sizeof(buffer) - strlen(buffer) - 1);
        first = 0;
    }
    if (attributes & ATTR_SYSTEM) {
        strncat(buffer, first ? "system" : ",system", sizeof(buffer) - strlen(buffer) - 1);
        first = 0;
    }
    if (attributes & ATTR_ARCHIVE) {
        strncat(buffer, first ? "archive" : ",archive", sizeof(buffer) - strlen(buffer) - 1);
        first = 0;
    }
    
    return buffer;
}