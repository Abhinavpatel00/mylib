#include "flow.h"
#include <stdio.h>


int main()
{

    uint32_t a = 5;
    uint32_t b = 2;

    FLOW_SWAP(uint32_t, a, b);

    printf("%u ", a);
    printf("%u", b);
    return 0;
}
