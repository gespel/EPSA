#include <signal.h>

int main() {
    raise(SIGSEGV);
    return 0;
}
