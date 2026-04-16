#include <stdlib.h>

int main(void) {
    /* Intentional leak: malloc without free so Valgrind reports "definitely lost". */
    int *p = malloc(sizeof(int) * 25);
    p[0]   = 42;
    return 0;
}
