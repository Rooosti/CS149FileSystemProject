#include "fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static node_t *root;
static node_t *cwd;

static node_t *node_new(node_type t, const char *name, node_t *parent) {
    node_t *n = calloc(1, sizeof(*n));
    n->type = t; n->parent = parent;
    strncpy(n->name, name, NAME_MAX);
    return n;
}

void fs_init(void) {
    root = node_new(N_DIR, "", NULL);
    cwd = root;
}

static void node_free(node_t *n) {
    if (!n) return;
    if (n->type == N_DIR)
        for (size_t i=0;i<n->child_count;i++) node_free(n->children[i]);
    free(n->data);
    free(n);
}

void fs_destroy(void) { node_free(root); root=NULL; }

static node_t *dir_add(node_t *dir, node_t *child) {
    if (!dir || dir->type!=N_DIR || dir->child_count>=MAX_CHILDREN) return NULL;
    dir->children[dir->child_count++] = child;
    return child;
}

static node_t *dir_find(node_t *dir, const char *name) {
    if (!dir || dir->type!=N_DIR) return NULL;
    for (size_t i=0;i<dir->child_count;i++)
        if (strncmp(dir->children[i]->name, name, NAME_MAX)==0) return dir->children[i];
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

int create_file(const char *path) {
    char leaf[NAME_MAX+1]={0};
    node_t *parent = walk(path, 1, leaf);
    if (!parent || parent->type!=N_DIR) return -1;
    if (dir_find(parent, leaf)) return -1; // exists
    node_t *f = node_new(N_FILE, leaf, parent);
    return dir_add(parent, f) ? 0 : -1;
}

static int ensure_cap(node_t *f, size_t want) {
    if (f->cap >= want) return 0;
    size_t newcap = f->cap ? f->cap : 64;
    while (newcap < want) newcap *= 2;
    uint8_t *p = realloc(f->data, newcap);
    if (!p) return -1;
    // zero new region
    if (newcap > f->cap) memset(p + f->cap, 0, newcap - f->cap);
    f->data = p; f->cap = newcap;
    return 0;
}

ssize_t write_file(const char *path, size_t off, const void *buf, size_t len) {
    node_t *f = walk(path, 0, NULL);
    if (!f || f->type!=N_FILE) return -1;
    size_t need = off + len;
    if (ensure_cap(f, need) < 0) return -1;
    memcpy(f->data + off, buf, len);
    if (need > f->size) f->size = need;
    return (ssize_t)len;
}

ssize_t read_file(const char *path, size_t off, void *buf, size_t len) {
    node_t *f = walk(path, 0, NULL);
    if (!f || f->type!=N_FILE) return -1;
    if (off >= f->size) return 0;
    size_t n = f->size - off; if (n > len) n = len;
    memcpy(buf, f->data + off, n);
    return (ssize_t)n;
}

int rm_file(const char *path) {
    char leaf[NAME_MAX+1]={0};
    node_t *parent = walk(path, 1, leaf);
    if (!parent) return -1;
    for (size_t i=0;i<parent->child_count;i++) {
        node_t *c = parent->children[i];
        if (strncmp(c->name, leaf, NAME_MAX)==0 && c->type==N_FILE) {
            // remove by swapping with last
            node_free(c);
            parent->children[i] = parent->children[parent->child_count-1];
            parent->child_count--;
            return 0;
        }
    }
    return -1;
}

int rmdir_empty(const char *path) {
    node_t *d = walk(path, 0, NULL);
    if (!d || d->type!=N_DIR || d==root) return -1;
    if (d->child_count) return -1;
    node_t *p = d->parent;
    for (size_t i=0;i<p->child_count;i++) if (p->children[i]==d) {
        node_free(d);
        p->children[i] = p->children[p->child_count-1];
        p->child_count--;
        return 0;
    }
    return -1;
}

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

    for (size_t i = 0; i < d->child_count; i++) {
        node_t *c = d->children[i];
        printf("%s%s\n", c->name, c->type == N_DIR ? "/" : "");
    }
    return 0;
}

