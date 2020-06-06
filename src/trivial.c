/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototypes or constants in a header, please put them
 * in another header file.
 *
 * When we grade, we will be replacing this file with our own copy.
 * You have been warned.
 * === DO NOT MODIFY THIS FILE ===
 */

/*
 * "Trivial" problem type.
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "debug.h"
#include "polya.h"

static struct problem *trivial_construct();
static void trivial_vary(struct problem *prob, int var);
static struct result *trivial_solve(struct problem *aprob, volatile sig_atomic_t *canceledp);
static int trivial_check(struct result *result, struct problem *prob);

/*
 * Initialize the trivial solver.
 */
struct solver_methods trivial_solver_methods = {
   trivial_construct, trivial_vary, trivial_solve, trivial_check
};

void trivial_solver_init(void) {
    solvers[TRIVIAL_PROBLEM_TYPE] = trivial_solver_methods;
}

/*
 * Construct a trivial problem.
 *
 * @param id     The problem ID.
 * @param nvars  The number of possible variants of the problem.
 */
static struct problem *trivial_construct(int id, int nvars) {
    struct problem *prob = malloc(sizeof(struct problem));
    if(prob == NULL)
	return NULL;
    prob->size = sizeof(struct problem);
    prob->type = TRIVIAL_PROBLEM_TYPE;
    prob->nvars = nvars;
    prob->id = id;
    return prob;
}

/*
 * Create a variant of a trivial problem.
 * This function does nothing.
 *
 * @param prob  The problem to be modified.
 * @param var  Integer in the range [0, nvars) specifying the particular variant
 * form to be created.
 */
static void trivial_vary(struct problem *prob, int var) {
    // Does nothing.
    return;
}

/*
 * Solve a trivial "problem", returning a result.
 * No actual work is done, but a successful result (with no data) is returned.
 *
 * @param aprob  Pointer to a structure describing the problem to be solved.
 * @param canceledp  Pointer to a flag indicating whether the current solution attempt
 * is to be canceled.
 * @return  Either a result structure with a "failed" field equal to zero is returned,
 * or NULL is returned if such a structure could not be created.  The caller is
 * responsible for freeing any non-NULL pointer that is returned.
 */
static struct result *trivial_solve(struct problem *aprob, volatile sig_atomic_t *canceledp) {
    debug("[%d:Worker] Trivial solver (id = %d)", getpid(), aprob->id);
    struct result *result = malloc(sizeof(struct result));
    if(result == NULL)
	return NULL;
    memset(result, 0, sizeof(*result));
    result->size = sizeof(*result);
    debug("[%d:Worker] Returning result (size = %ld, failed = %d)",
	  getpid(), result->size, result->failed);
    return result;
}

/*
 * Check a solution to a trivial problem.
 *
 * @param result  The result to be checked.
 * @param prob  The problem the result is supposed to solve.
 * @return -1 if the result is marked "failed", otherwise 0.
 */
static int trivial_check(struct result *result, struct problem *prob) {
    if(result->failed)
	return -1;
    return 0;
}
