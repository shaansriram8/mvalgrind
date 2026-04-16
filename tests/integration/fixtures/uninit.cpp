#include <cstdio>

int main(void) {
    int x;
    /* Read from an uninitialised variable — Valgrind should flag this. */
    if (x > 0) {
        printf("positive\n");
    }
    return 0;
}
