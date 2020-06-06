#include <stdlib.h>

#include "debug.h"
#include "polya.h"
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

volatile sig_atomic_t sig_term = 0;

volatile sig_atomic_t sigh_up = 0;

void sig_term_handler(int signum) {
    sig_term = 1;
    exit(EXIT_SUCCESS);
}

void sigh_up_handler(int signum) {
    sigh_up = 1;
}
/*
 * worker
 * (See polya.h for specification.)
 */
int worker(void) {
    //debug("WOrker beginsssss");
    signal(SIGTERM, sig_term_handler);
    signal(SIGHUP, sigh_up_handler);

    for(;;){

    //debug("Inside loop in worker");
        pid_t current_pid = getpid();

        kill(current_pid, SIGSTOP);
        struct problem *prob = malloc(sizeof(struct problem));
        fread(prob, sizeof(struct problem), 1, stdin);
        //debug("Now will read problem in worker");
        prob = realloc(prob,prob->size);
        fread(prob->data,(prob->size)- sizeof(struct problem),1, stdin);

        struct result *answer = solvers[prob->type].solve(prob, &sigh_up);
        if(answer ==  NULL){
            answer = malloc(sizeof(struct result));
            answer->size = sizeof(struct result);
            answer->failed = 1;
        }
        fwrite(answer,answer->size, 1, stdout);
        fflush(stdout);
        //debug("Finished writing result");
        free(prob);
        free(answer);

    }

    return EXIT_FAILURE;
}
