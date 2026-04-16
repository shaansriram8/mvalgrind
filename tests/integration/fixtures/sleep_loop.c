#include <unistd.h>

int main(void) {
    /* Used by the SIGINT cleanup integration test. */
    while (1) sleep(1);
    return 0;
}
