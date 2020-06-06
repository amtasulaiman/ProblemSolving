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
 * This module implements a source of problems to be solved.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

#include "debug.h"
#include "polya.h"

static void new_problem(int type, int nvars);

/* The problem currently being solved (in its several variant forms). */
static struct problem *current_problem;

/* Access to methods of the various solver types. */
struct solver_methods solvers[NUM_PROBLEM_TYPES];

/* Number of problems yet to be generated. */
static int problems_remaining;

/* Number of enabled problem types. */
static int num_problem_types;

/* Bit mask controlling which types of problems are generated. */
static unsigned int prob_type_mask;

/* Table of solver initialization functions. */
extern void trivial_solver_init(void);
extern void crypto_miner_solver_init(void);

void (*solver_initializers[NUM_PROBLEM_TYPES])(void) = {
    NULL, trivial_solver_init, crypto_miner_solver_init
};

/*
 * init_problems
 * (See polya.h for specification.)
 */
void init_problems(int nprobs, unsigned int mask) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    srandom(tv.tv_usec);
    problems_remaining = nprobs;
    prob_type_mask = mask;
    for(int i = 0; i < NUM_PROBLEM_TYPES; i++) {
	if(mask & (1 << i) && solver_initializers[i]) {
	    num_problem_types++;
	    (*solver_initializers[i])();
	}
    }
}

/*
 * get_problem_variant
 * (See polya.h for specification.)
 */
struct problem *get_problem_variant(int nvars, int var) {
    struct problem *prob = current_problem;
    if(prob == NULL && num_problem_types > 0) {
	// Select an enabled problem type at random.
	while(problems_remaining && current_problem == NULL) {
	    int type = random() % NUM_PROBLEM_TYPES;
	    if(solvers[type].construct)
		new_problem(type, nvars);
	}
    }
    prob = current_problem;
    if(prob == NULL) {
	debug("[%d:Master] No more problems", getpid());
	return NULL;
    }
    if(var < 0 || (prob->nvars && var >= prob->nvars)) {
	debug("[%d:Master] Invalid problem variant", getpid());
	return NULL;
    }
    if(solvers[prob->type].vary == NULL) {
	debug("[%d:Master] No varier for problem type %d", getpid(), prob->type);
	return NULL;
    }
    (*solvers[prob->type].vary)(prob, var);
    return prob;
}

/*
 * Create a new problem, of a specified type and with a specified
 * number of possible variant forms.
 *
 * @param type  The type of problem to be created.
 * @param nvars  The number of possible variant forms of the problem.
 */
static void new_problem(int type, int nvars) {
    debug("[%d:Master] Create new problem: type = %d, nvars = %d", getpid(), type, nvars);
    if(current_problem) {
	free(current_problem);
	current_problem = NULL;
    }
    static int id = 0;
    if(problems_remaining-- > 0) {
	++id;
	debug("[%d:Master] Generating problem, number remaining: %d", getpid(), problems_remaining);
	switch(type) {
	case TRIVIAL_PROBLEM_TYPE:
	    current_problem = solvers[type].construct(id, nvars);
	    return;
	case CRYPTO_MINER_PROBLEM_TYPE:
	    {
		char block[32];
		// Generate random block data.
		for(int i = 0; i < sizeof(block); i++)
		    block[i] = random() & 0xff;
		current_problem = solvers[type].construct(id, nvars, block, sizeof(block), 8, 25);
	    }
	    return;
	default:
	    return;
	}
    }
    current_problem = NULL;
}

/*
 * post_result
 * (See polya.h for specification.)
 */
int post_result(struct result *result, struct problem *prob) {
    debug("[%d:Master] Post result %p to problem %p", getpid(), result, prob);
    int type = prob->type;
    if(result->failed)
	return -1;
    if(!solvers[type].check(result, prob)) {
	debug("[%d:Master] Result is correct!", getpid());
	if(current_problem == prob) {
	    debug("[%d:Master] Clearing current problem, which is now solved", getpid());
	    current_problem = NULL;
	    free(prob);
	}
	return 0;
    } else {
	debug("[%d:Master] Posted result does not solve the problem", getpid());
	return 1;
    }
}
