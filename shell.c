#include "fs.h"
#include <stdio.h>
#include <string.h>

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
        printf("%s\n", mkdir_p(p1) ? "err" : "ok");
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
        printf("%s\n", create_file(p1) ? "err" : "ok");
    }

    else if (!strcmp(cmd, "cd")) {
        if (n < 2) {
            // pick your favorite: root, or stay in place
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
            puts("err");
        }
    }

    else if (!strcmp(cmd,"rm")) {
        if (n < 2) { printf("usage: rm PATH\n"); continue; }
        printf("%s\n", rm_file(p1) ? "err" : "ok");
    }

    else if (!strcmp(cmd,"rmdir")) {
        if (n < 2) { printf("usage: rmdir PATH\n"); continue; }
        printf("%s\n", rmdir_empty(p1) ? "err" : "ok");
    }

    else puts("Unknown Command");
    }
}
