#include <criterion/criterion.h>
#include <criterion/logging.h>

#include "polya.h"

/*
 * These tests just test the student worker using the demo master.
 * This should expose issues with the worker, since the demo master
 * will track whether the workers crash and whether they actually
 * solve any problems.
 *
 * Tests to be used in grading will track the behavior of the master
 * via the event functions that it is to call.  No such tests are
 * included here with the basecode.
 */

Test(demo_master_suite, startup_test) {
    char *cmd = "demo/polya";
    int return_code = WEXITSTATUS(system(cmd));

    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program exited with %d instead of EXIT_SUCCESS",
		 return_code);
}

Test(demo_master_suite, trivial_test) {
    char *cmd = "demo/polya -p 1 -t 1";
    int return_code = WEXITSTATUS(system(cmd));

    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program exited with %d instead of EXIT_SUCCESS",
		 return_code);
}

Test(demo_master_suite, miner_test_one_worker) {
    char *cmd = "demo/polya -p 5 -t 2";
    int return_code = WEXITSTATUS(system(cmd));

    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program exited with %d instead of EXIT_SUCCESS",
		 return_code);
}

Test(demo_master_suite, miner_test_three_workers) {
    char *cmd = "demo/polya -p 5 -t 2 -w 3";
    int return_code = WEXITSTATUS(system(cmd));

    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program exited with %d instead of EXIT_SUCCESS",
		 return_code);
}
