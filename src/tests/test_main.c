#include "test.h"

int failures;
int checks;

int main(void)
{
    test_core();
    test_alu();
    test_flow();
    test_draw();
    test_mem();
    test_io();
    test_quirks();
    test_disasm();
    test_state();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
