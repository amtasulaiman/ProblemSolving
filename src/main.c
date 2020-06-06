/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototypes or constants in a header, please put them
 * in another header file.
 *
 * When we grade, we will be replacing this file with our own copy.
 * You have been warned.
 * === DO NOT MODIFY THIS FILE ===
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>

#include "debug.h"
#include "polya.h"

/*
 * "Polya" multiprocess problem solver: master process.
 *
 * Usage:
 *   polya [-w num_workers] [-p num_probs] [-t prob_type]
 *
 * where:
 *   num_workers is the number of workers to use (min 1, max 32, default 1)
 *   num_probs is the total number of problems to be solved (default 0)
 *   prob_type is an integer specifying a problem type whose solver
 *     is to be enabled (min 0, max 31).  The -t flag may be repeated to enable
 *     multiple problem types.
 */
int main(int argc, char *argv[])
{
    int nworkers = 1;
    int nprobs = 0;
    unsigned int mask = 0;
    int type, option;
    while((option = getopt(argc, argv, "w:p:t:")) != EOF) {
	switch(option) {
	case 'w':
	    if((nworkers = atoi(optarg++)) <= 0 || nworkers >= 32) {
		fprintf(stderr, "-w (workers) requires argument in range [1..31]\n");
		exit(EXIT_FAILURE);
	    }
	    break;
	case 'p':
	    if((nprobs = atoi(optarg++)) < 0) {
		fprintf(stderr, "-p (problems) requires a nonnegative argument\n");
		exit(EXIT_FAILURE);
	    }
	    break;
	case 't':
	    type = atoi(optarg++);
	    if(type < 0 || type >= NUM_PROBLEM_TYPES) {
		fprintf(stderr, "-t (problem type) requires an argument in range [0..%d]\n",
			NUM_PROBLEM_TYPES-1);
		exit(EXIT_FAILURE);
	    }
	    mask |= (1 << type);
	    break;
	default:
	    fprintf(stderr, "Unknown option\n");
	    exit(EXIT_FAILURE);
	}
    }
    init_problems(nprobs, mask);
    return master(nworkers);
}
