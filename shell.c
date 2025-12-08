#include "fs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void){
    fs_init();
    char line[1024];
    
while (printf("fsh> "), fflush(stdout), fgets(line, sizeof(line), stdin)) {
    char cmd[32], p1[512], p2[512];
    int n = sscanf(line, "%31s %511s %511[^\n]", cmd, p1, p2);
    if (n < 1) continue;  // no command

    if (!strcmp(cmd,"exit")) break;

    else if (!strcmp(cmd,"mkdir")) {
        if (n < 2) { printf("usage: mkdir PATH\n"); continue; }
        printf("%s\n", mkdir_p(p1) ? "Could not make directory" : "Successfuly created directory!");
    }

    else if (!strcmp(cmd,"ls")) {
        if (n < 2) {
            // no path → use cwd
            (void)ls_dir(NULL);
        } else {
            (void)ls_dir(p1);
        }
    }

    else if (!strcmp(cmd,"create")) {
        if (n < 2) { printf("usage: create PATH\n"); continue; }
        printf("%s\n", create_file(p1) ? "Could not create file" : "Successfuly created file!");
    }

    else if (!strcmp(cmd, "cd")) {
        if (n < 2) {
            (void)fs_cd("/");
        } else {
            (void)fs_cd(p1);
        }
    }

    else if (!strcmp(cmd,"write")) {
        if (n < 3) { printf("usage: write PATH DATA...\n"); continue; }
        printf("%zd\n", write_file(p1, 0, p2, strlen(p2)));
    }

    else if (!strcmp(cmd,"read")) {
        if (n < 2) { printf("usage: read PATH\n"); continue; }
        char buf[1024] = {0};
        ssize_t r = read_file(p1, 0, buf, sizeof(buf)-1);
        if (r >= 0) {
            buf[r] = 0;
            printf("%s", buf);
            if (r > 0 && buf[r-1] != '\n') puts("");
        } else {
            puts("Error reading file");
        }
    }

    else if (!strcmp(cmd,"rm")) {
        if (n < 2) { printf("usage: rm PATH\n"); continue; }
        printf("%s\n", rm_file(p1) ? "Could not remove file" : "Successfuly removed file!");
    }

    else if (!strcmp(cmd,"rmdir")) {
        if (n < 2) { printf("usage: rmdir PATH\n"); continue; }
        printf("%s\n", rmdir_empty(p1) ? "Error removing directory. Please check if empty" : "Successfuly removed directory!");
    }

    else if (!strcmp(cmd,"info")) {
        if (n < 2) { printf("usage: info PATH\n"); continue; }
        file_info_t info;
        if (get_file_info(p1, &info) == 0) {
            printf("Name: %s\n", info.name);
            printf("Type: %s\n", info.type == N_FILE ? "File" : "Directory");
            if (info.type == N_FILE) {
                printf("Size: %zu bytes\n", info.size);
            } else {
                printf("Children: %zu\n", info.child_count);
            }
            printf("Created: %s\n", format_time(info.created));
            printf("Modified: %s\n", format_time(info.modified));
            printf("Accessed: %s\n", format_time(info.accessed));
            printf("Attributes: %s\n", format_attributes(info.attributes));
        } else {
            puts("Error reading file metadata");
        }
    }

    else if (!strcmp(cmd,"attr")) {
        if (n < 3) { printf("usage: attr PATH FLAGS\n"); continue; }
        int attrs = atoi(p2);
        printf("%s\n", set_file_attributes(p1, (uint8_t)attrs) ? "Error changing attributes" : "Successfully changed file attributes1");
    }

    else if (!strcmp(cmd,"touch")) {
        if (n < 2) { printf("usage: touch PATH\n"); continue; }
        printf("%s\n", touch_file(p1) ? "Error touching file" : "Successfully touched file");
    }
    
    else if (!strcmp(cmd,"rename") || !strcmp(cmd,"mv")) {
        if (n < 3) { printf("usage: %s OLD_PATH NEW_PATH\n", cmd); continue; }
        printf("%s\n", rename_file(p1, p2) ? "Error renaming file" : "Successfully renamed/moved file");
    }
    
    else if (!strcmp(cmd,"open")) {
        if (n < 3) { printf("usage: open PATH MODE (r/w/rw)\n"); continue; }
        int flags = 0;
        if (strcmp(p2, "r") == 0) flags = O_RDONLY;
        else if (strcmp(p2, "w") == 0) flags = O_WRONLY;
        else if (strcmp(p2, "rw") == 0) flags = O_RDWR;
        else { printf("Invalid mode. Use r, w, or rw\n"); continue; }
        
        int fd = fs_open(p1, flags);
        if (fd >= 0) {
            printf("File opened with descriptor: %d\n", fd);
        } else {
            printf("Error opening file\n");
        }
    }
    
    else if (!strcmp(cmd,"close")) {
        if (n < 2) { printf("usage: close FD\n"); continue; }
        int fd = atoi(p1);
        printf("%s\n", fs_close(fd) ? "Error closing file" : "Successfully closed file");
    }
    
    else if (!strcmp(cmd,"readfd")) {
        if (n < 2) { printf("usage: readfd FD [LENGTH]\n"); continue; }
        int fd = atoi(p1);
        size_t len = 1024;
        if (n >= 3) len = (size_t)atoi(p2);
        
        char *buf = malloc(len + 1);
        if (!buf) { printf("Memory allocation error\n"); continue; }
        
        ssize_t r = fs_read_fd(fd, buf, len);
        if (r >= 0) {
            buf[r] = '\0';
            printf("%s", buf);
            if (r > 0 && buf[r-1] != '\n') puts("");
        } else {
            printf("Error reading from file descriptor\n");
        }
        free(buf);
    }
    
    else if (!strcmp(cmd,"writefd")) {
        if (n < 3) { printf("usage: writefd FD DATA\n"); continue; }
        int fd = atoi(p1);
        ssize_t w = fs_write_fd(fd, p2, strlen(p2));
        if (w >= 0) {
            printf("Wrote %zd bytes\n", w);
        } else {
            printf("Error writing to file descriptor\n");
        }
    }
    
    else if (!strcmp(cmd,"seek")) {
        if (n < 3) { printf("usage: seek FD OFFSET [WHENCE]\n"); continue; }
        int fd = atoi(p1);
        off_t offset = (off_t)atoll(p2);
        int whence = 0; // SEEK_SET
        if (n >= 4) whence = atoi(line + strlen(cmd) + strlen(p1) + strlen(p2) + 3);
        
        off_t pos = fs_seek(fd, offset, whence);
        if (pos >= 0) {
            printf("New position: %lld\n", (long long)pos);
        } else {
            printf("Error seeking\n");
        }
    }
    
    else if (!strcmp(cmd,"search")) {
        if (n < 2) {
            printf("usage: search TERM\n");
            continue;
        }

        int matches = fs_search(p1);
        if (matches < 0) {
            puts("Error: fs_search returned negative count for matches?");
        } else if (matches == 0) {
            puts("(no matches)");
        }
    }

    else if (!strcmp(cmd,"help")) {
        puts("Commands:");
        puts("  File & Directory Management:");
        puts("    mkdir PATH             - create directory");
        puts("    ls [PATH]              - list directory");
        puts("    create PATH            - create file");
        puts("    cd PATH                - change directory");
        puts("    rm PATH                - remove file");
        puts("    rmdir PATH             - remove directory");
        puts("    rename OLD NEW         - rename/move file or directory");
        puts("    mv OLD NEW             - alias for rename");
        puts("");
        puts("  File I/O (Path-based):");
        puts("    write PATH TEXT        - write to file");
        puts("    read PATH              - read file");
        puts("");
        puts("  File I/O (Descriptor-based):");
        puts("    open PATH MODE         - open file (mode: r/w/rw) returns FD");
        puts("    close FD               - close file descriptor");
        puts("    readfd FD [LEN]        - read from file descriptor");
        puts("    writefd FD TEXT        - write to file descriptor");
        puts("    seek FD OFFSET [WHE]   - seek in file (whence: 0=SET,1=CUR,2=END)");
        puts("");
        puts("  Metadata & Search:");
        puts("    info PATH              - show file/directory metadata");
        puts("    attr PATH FLAGS        - set attributes (0-15)");
        puts("    touch PATH             - update timestamps");
        puts("    search TERM            - find files matching term");
        puts("");
        puts("  System:");
        puts("    help                   - show this help");
        puts("    exit                   - quit");
    }

    else puts("Unknown Command");
    }
}
