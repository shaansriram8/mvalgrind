#include <stdlib.h>

int main(void) {
    int *p = malloc(sizeof(int) * 25);
    p[0]   = 42;
    free(p);
    return 0;
}
