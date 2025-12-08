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
        puts("  mkdir PATH - create directory");
        puts("  ls [PATH] - list directory");
        puts("  create PATH - create file");
        puts("  cd PATH - change directory");
        puts("  write PATH TEXT - write to file");
        puts("  read PATH - read file");
        puts("  rm PATH - remove file");
        puts("  rmdir PATH - remove directory");
        puts("  info PATH - show metadata");
        puts("  attr PATH FLAGS - set attributes");
        puts("  touch PATH - update timestamps");
        puts("  search TERM - find paths containing a term");
        puts("  help - show this help");
        puts("  exit - quit");
    }

    else puts("Unknown Command");
    }
}
