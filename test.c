#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    printf("Hello, World!\n");
    const char* pwd = getenv("USER");
    printf("USER: %s\n", pwd);
    system("id");
    sleep(2);
    return 0;
}
