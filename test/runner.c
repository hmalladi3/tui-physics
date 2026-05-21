#include "test.h"

int         g_test_pass     = 0;
int         g_test_fail     = 0;
int         g_current_failed = 0;
const char* g_current_test  = "";

void run_physics_tests(void);
void run_render_tests(void);
void run_term_tests(void);
void run_scenes_tests(void);

int main(void) {
    printf("physics:\n");
    run_physics_tests();
    printf("render:\n");
    run_render_tests();
    printf("term:\n");
    run_term_tests();
    printf("scenes:\n");
    run_scenes_tests();
    printf("\n%d passed, %d failed\n", g_test_pass, g_test_fail);
    return g_test_fail == 0 ? 0 : 1;
}
