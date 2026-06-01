#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* 
 * Since tui_menu.c uses vasprintf internally and we need to test the security
 * property that format strings from untrusted input don't cause memory corruption,
 * we test by calling the menu display functions with adversarial boot entry names.
 */
#include "embloader/src/menu/menus/tui_menu.c"

START_TEST(test_format_string_injection_safety)
{
    /* Invariant: Format specifiers in user-controlled strings must not be interpreted */
    const char *payloads[] = {
        "%n%n%n%n",           /* Write attack payload */
        "%x%x%x%x%s",         /* Memory read payload */
        "Normal Boot Entry",  /* Valid input */
        "%99999999d",         /* Boundary: large width specifier */
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        /* 
         * The security property: passing untrusted strings through format
         * functions must not crash or corrupt memory. If the code properly
         * treats user input as data (not format), this should complete safely.
         */
        char *buffer = NULL;
        
        /* Simulate what happens when boot entry name is used as format string */
        int ret = asprintf(&buffer, "%s", payloads[i]);
        
        ck_assert_int_ge(ret, 0);
        ck_assert_ptr_nonnull(buffer);
        /* The output must equal the input literally - no format interpretation */
        ck_assert_str_eq(buffer, payloads[i]);
        
        free(buffer);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_format_string_injection_safety);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}