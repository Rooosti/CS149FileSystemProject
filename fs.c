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
    n->type = t; 
    n->parent = parent;
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
void fs_destroy(void) { 

    // Free nodes starting from root.
    node_free(root); 
    root = NULL; 

    // Make sure to reassign CWD to NULL!
    cwd = NULL;
}

// Directory management helper functions:
// Add a child to a directory.
static node_t *dir_add(node_t *dir, node_t *child) {

    // Validate that passed in directory is not null, that the node is a directory, and the directory is not full.
    if (!dir || dir->type!=N_DIR || dir->child_count>=MAX_CHILDREN) return NULL;

    // Validate the child pointer is not null.
    if (!child) return NULL;

    // Prevent attaching a node that already belongs to another directory.
    if (child->parent && child->parent != dir) return NULL;

    // Prevent adding the same child twice.
    for (size_t i = 0; i < dir->child_count; i++) {
        if (dir->children[i] == child) {
            return dir->children[i];
        }
    }
    // Put the child into the children array of the directory and increment the counter.
    dir->children[dir->child_count++] = child;

    // Set the child's parent pointer to point back to dir (tree is bidirectional).
    child->parent = dir;

    // Update directory metadata.
    dir->modified = dir->accessed = time(NULL);

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

// Build full path for a node into buffer (including leading '/').
static void node_get_path(node_t *n, char *buf, size_t bufsize) {
    if (!n || !buf || bufsize == 0) {
        return;
    }

    // case when just "/"
    if (n == root) {
        if (bufsize > 1) {
            buf[0] = '/';
            buf[1] = '\0';
        } else {
            buf[0] = '\0';
        }
        return;
    }

    // Collect segments by walking up to root
    const char *segments[64];
    size_t count = 0;

    node_t *cur = n;
    while (cur && cur != root && count < 64) {
        segments[count++] = cur->name;
        cur = cur->parent;
    }

    size_t pos = 0;

    // Leading slash
    if (pos < bufsize - 1) {
        buf[pos++] = '/';
    }

    // Append segments in reverse (from closest to root)
    for (size_t i = 0; i < count && pos < bufsize - 1; i++) {
        const char *name = segments[count - 1 - i];
        size_t len = strlen(name);


        // Truncate if necessary
        if (pos + len >= bufsize - 1) {
            len = (bufsize - 1) - pos;
        }

        memcpy(buf + pos, name, len);
        pos += len;

        if (i + 1 < count && pos < bufsize - 1) {
            buf[pos++] = '/';
        }
    }
    buf[pos] = '\0';
}


// Recursive helper: search subtree rooted at `n` for names containing `term`.
// Prints full paths of matches. Returns number of matches.
static int search_subtree(node_t *n, const char *term) {
    if (!n || !term || term[0] == '\0') return 0;

    int matches = 0;

    // Visiting this node counts as an access.
    n->accessed = time(NULL);

    // Skip root's empty name when matching.
    if (n != root && strstr(n->name, term) != NULL) {
        char path[1024];
        node_get_path(n, path, sizeof(path));
        printf("%s%s\n", path, n->type == N_DIR ? "/" : "");
        matches++;
    }

    // Recurse into children if this is a directory.
    if (n->type == N_DIR) {
        for (size_t i = 0; i < n->child_count; i++) {
            matches += search_subtree(n->children[i], term);
        }
    }

    return matches;
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
    
    // Update access time since we accessed the directory.
    d->accessed = time(NULL);
    
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
            } else {
                // Update accessed time when traversing through existing directory.
                n->accessed = time(NULL);
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
    char leaf[NAME_MAX + 1] = {0};

    // Use want_parent = 1 to get the parent directory. 
    // For example, for "/documents/myfile.txt", returns "documents/" and puts "myfile.txt" in leaf.
    node_t *parent = walk(path, 1, leaf);

    // Validate that the parent exists and it is a directory node.
    if (!parent || parent->type!=N_DIR) return -1;

    // Validate leaf before creation (some of these are already checked by shell.c and other fs.c functions, but we want to make our program more robust).
    if (leaf[0] == '\0') return -1; // Don't want to create files with empty names.
    if (strcmp(leaf, ".") == 0 || strcmp(leaf, "..") == 0) return -1; // Prevent "." and ".." as file names.
    if (strlen(leaf) > NAME_MAX) return -1; // Prevent names that are too long.

    // Prevent duplicate file creation so that we don't overwrite.
    if (dir_find(parent, leaf)) return -1;

    // Prevent file creation if parent directory is READ_ONLY.
    if (parent->attributes & ATTR_READONLY) return -1;

    // Create file node.
    node_t *f = node_new(N_FILE, leaf, parent);
    if (!dir_add(parent, f)) {
        node_free(f);
        return -1;
    }

    // Though file metadata is handled by node_new(), we need to update the parent directory metadata.
    time_t now = time(NULL);
    parent->modified = parent->accessed = now;

    // Return result.
    return 0;
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

// Implements file write operation at a specific offset.
// path: file path.
// off: byte offset indicating where to start writing.
// buf: pointer to data to write.
// len: number of bytes to write.
ssize_t write_file(const char *path, size_t off, const void *buf, size_t len) {

    // Find the file to write to using walk() & want_parent = 0, which will return actual file node.
    node_t *f = walk(path, 0, NULL);

    // Validate that the file exists and is not a directory.
    if (!f || f->type!=N_FILE) return -1;

    if (f->attributes & ATTR_READONLY) return -1;

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

    // Prevent removal of a file in a READ_ONLY directory.
    if (parent->attributes & ATTR_READONLY) return -1;

    // Search for the file in the parent directory using linear search on the parent directory's children array.
    for (size_t i = 0; i < parent->child_count; i++) {
        node_t *c = parent->children[i];

        // Name must match filename (leaf) and the node type must be a file, not a directory.
        if (strncmp(c->name, leaf, NAME_MAX) == 0 && c->type == N_FILE) {

            // Prevent removal of a READ_ONLY file.
            if (c->attributes & ATTR_READONLY) return -1;
            
            // Perform removal using swap-with-last algorithm.
            // This removes in O(1), doesn't shift other files, and only requires a copy operation.
            // However, it does not preserve file position/order in the directory listing.
            
            // IMPORTANT: Swap first, then free to avoid use-after-free bug.
            parent->children[i] = parent->children[parent->child_count-1];
            parent->child_count--;
            node_free(c);  // Now safe to free.

            // Update parent metadata for modification time and also accessed time.
            parent->modified = parent->accessed = time(NULL);

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
    
    // Prevent removal of a directory in a READ_ONLY parent directory.
    if (p->attributes & ATTR_READONLY) return -1;
    
    // Prevent removal of a READ_ONLY directory.
    if (d->attributes & ATTR_READONLY) return -1;

    // Find and remove directory using swap-with-last algorithm (linear search over children).
    for (size_t i = 0; i < p->child_count; i++) {
        
        // Use pointer comparison instead of name comparison.
        if (p->children[i] == d) { 

            // Swap first, then free node!
            p->children[i] = p->children[p->child_count-1];
            p->child_count--;
            node_free(d);

            // Update parent metadata.
            p->modified = p->accessed = time(NULL);

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

int fs_search(const char *term) {
    if (!term || term[0] == '\0') return -1;
    int matches = search_subtree(cwd, term);
    return matches >= 0 ? matches : -1;
}

// Rename or move a file/directory.
int rename_file(const char *old_path, const char *new_path) {
    if (!old_path || !new_path) return -1;
    
    // Find the node to rename.
    node_t *n = walk(old_path, 0, NULL);
    if (!n) return -1;
    
    // Can't rename root.
    if (n == root) return -1;
    
    // Check if source is readonly.
    if (n->attributes & ATTR_READONLY) return -1;
    if (n->parent && (n->parent->attributes & ATTR_READONLY)) return -1;
    
    // Parse new path to get new parent and new name.
    char new_name[NAME_MAX + 1] = {0};
    node_t *new_parent = walk(new_path, 1, new_name);
    
    if (!new_parent || new_parent->type != N_DIR) return -1;
    
    // Check if new parent is readonly.
    if (new_parent->attributes & ATTR_READONLY) return -1;
    
    // Check if name already exists in new parent.
    if (dir_find(new_parent, new_name)) return -1;
    
    // If moving to different directory, remove from old parent.
    if (n->parent != new_parent) {
        node_t *old_parent = n->parent;

        // Remove from old parent's children array (swap-with-last).
        for (size_t i = 0; i < old_parent->child_count; i++) {
            if (old_parent->children[i] == n) {
                old_parent->children[i] = old_parent->children[old_parent->child_count - 1];
                old_parent->child_count--;
                break;
            }
        }

        // Clear the parent pointer so dir_add can attach the node to the new parent.
        n->parent = NULL;

        // Try to add to new parent; on failure, restore to old parent.
        if (!dir_add(new_parent, n)) {
            if (!dir_add(old_parent, n)) return -1;
            return -1;
        }

        // Update old parent metadata after removal.
        old_parent->modified = old_parent->accessed = time(NULL);
    }
    
    // Update the name.
    strncpy(n->name, new_name, NAME_MAX);
    n->name[NAME_MAX] = '\0';
    
    // Update timestamps.
    time_t now = time(NULL);
    n->modified = now;
    if (n->parent) n->parent->modified = now;
    
    return 0;
}

// File descriptor table.
typedef struct {
    node_t *node;   // Pointer to file node.
    size_t offset;  // Current position in file.
    int flags;      // Access mode (O_RDONLY, O_WRONLY, O_RDWR).
    int in_use;     // Is this slot in use?
} file_descriptor_t;

static file_descriptor_t fd_table[MAX_OPEN_FILES];

// Open a file and return file descriptor.
int fs_open(const char *path, int flags) {
    // Find the file.
    node_t *n = walk(path, 0, NULL);
    if (!n || n->type != N_FILE) return -1;
    
    // Check permissions.
    if ((flags & O_WRONLY || flags & O_RDWR) && (n->attributes & ATTR_READONLY)) {
        return -1; // Can't open readonly file for writing.
    }
    
    // Find a free slot in fd table.
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!fd_table[i].in_use) {
            fd_table[i].node = n;
            fd_table[i].offset = 0;
            fd_table[i].flags = flags;
            fd_table[i].in_use = 1;
            
            // Update access time.
            n->accessed = time(NULL);
            
            return i;
        }
    }
    
    return -1; // No free file descriptors.
}

// Close a file descriptor.
int fs_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use) return -1;
    
    fd_table[fd].in_use = 0;
    fd_table[fd].node = NULL;
    fd_table[fd].offset = 0;
    fd_table[fd].flags = 0;
    
    return 0;
}

// Read from file descriptor.
ssize_t fs_read_fd(int fd, void *buf, size_t len) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use) return -1;
    if (!buf) return -1;
    
    file_descriptor_t *f = &fd_table[fd];
    
    // Check if file is open for reading.
    if (!(f->flags & O_RDONLY) && !(f->flags & O_RDWR)) return -1;
    
    node_t *n = f->node;
    if (!n || n->type != N_FILE) return -1;
    
    // Check for EOF.
    if (f->offset >= n->size) return 0;
    
    // Calculate bytes to read.
    size_t to_read = n->size - f->offset;
    if (to_read > len) to_read = len;
    
    // Copy data.
    memcpy(buf, n->data + f->offset, to_read);
    
    // Update offset and access time.
    f->offset += to_read;
    n->accessed = time(NULL);
    
    return (ssize_t)to_read;
}

// Write to file descriptor.
ssize_t fs_write_fd(int fd, const void *buf, size_t len) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use) return -1;
    if (!buf) return -1;
    
    file_descriptor_t *f = &fd_table[fd];
    
    // Check if file is open for writing.
    if (!(f->flags & O_WRONLY) && !(f->flags & O_RDWR)) return -1;
    
    node_t *n = f->node;
    if (!n || n->type != N_FILE) return -1;
    
    // Check if file is readonly.
    if (n->attributes & ATTR_READONLY) return -1;
    
    // Ensure capacity.
    size_t need = f->offset + len;
    if (ensure_cap(n, need) < 0) return -1;
    
    // Write data.
    memcpy(n->data + f->offset, buf, len);
    
    // Update size if we extended the file.
    if (need > n->size) n->size = need;
    
    // Update offset and timestamps.
    f->offset += len;
    time_t now = time(NULL);
    n->modified = now;
    n->accessed = now;
    
    return (ssize_t)len;
}

// Seek to a position in file.
off_t fs_seek(int fd, off_t offset, int whence) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use) return -1;
    
    file_descriptor_t *f = &fd_table[fd];
    node_t *n = f->node;
    if (!n || n->type != N_FILE) return -1;
    
    off_t new_offset;
    
    switch (whence) {
        case 0: // SEEK_SET
            new_offset = offset;
            break;
        case 1: // SEEK_CUR
            new_offset = (off_t)f->offset + offset;
            break;
        case 2: // SEEK_END
            new_offset = (off_t)n->size + offset;
            break;
        default:
            return -1;
    }
    
    if (new_offset < 0) return -1;
    
    f->offset = (size_t)new_offset;
    return new_offset;
}
