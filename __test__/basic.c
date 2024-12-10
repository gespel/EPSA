#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    printf("Hello, Test!\n");
    printf("USER: %s\n", getenv("USER"));
    system("id");

    sleep(10);

    return 0;
}
