#include "fs.h"
#include <stdio.h>
#include <string.h>

int main(void){
    fs_init();
    char line[1024];
    while (printf("fsh> "), fflush(stdout), fgets(line, sizeof(line), stdin)) {
        char cmd[32], p1[512], p2[512];
        if (sscanf(line, "%31s %511s %511[^\n]", cmd, p1, p2) < 1) continue;

        if (!strcmp(cmd,"exit")) break;
        else if (!strcmp(cmd,"mkdir"))      printf("%s\n", mkdir_p(p1)? "err":"ok");
        else if (!strcmp(cmd,"ls"))         (void)ls_dir(p1);
        else if (!strcmp(cmd,"create"))     printf("%s\n", create_file(p1)? "err":"ok");
        else if (!strcmp(cmd,"write"))      printf("%zd\n", write_file(p1, 0, p2, strlen(p2)));
        else if (!strcmp(cmd,"read")) {
            char buf[1024]={0};
            ssize_t n = read_file(p1, 0, buf, sizeof(buf)-1);
            if (n>=0) { buf[n]=0; printf("%s", buf); if (buf[n-1]!='\n') puts(""); }
            else puts("err");
        } else if (!strcmp(cmd,"rm"))       printf("%s\n", rm_file(p1)? "err":"ok");
        else if (!strcmp(cmd,"rmdir"))      printf("%s\n", rmdir_empty(p1)? "err":"ok");
        else puts("unknown");
    }
    fs_destroy();
    return 0;
}
