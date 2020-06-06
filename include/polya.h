/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototypes or constants in a header, please put them
 * in another header file.
 *
 * When we grade, we will be replacing this file with our own copy.
 * You have been warned.
 * === DO NOT MODIFY THIS FILE ===
 */
#ifndef POLYA_H
#define POLYA_H

/*
 * The "Polya" program is a general-purpose program for managing a number of
 * processes that are attempting to concurrently solve some kind of computationally
 * intensive problems.  When in operation, the program has a "master process",
 * which coordinates a number of "worker processes" that are doing the actual
 * solving.  The master process and the worker processes coordinate using
 * a protocol that involves sending "problems" and "results" over pipes, and
 * sending notifications to each other using signals.
 */

#include <signal.h>

/*
 * Problem types.
 * These are defined so that the Polya program is not limited to solving just
 * one particular kind of problem. The main Polya program manages the solution of
 * problems without needing to know anything about exactly what is being solved;
 * the details of particular problem types are left to individual "solver" modules.
 * Our primary example of a problem type is the "crypto miner" problem type,
 * in which the worker processes search for solutions to "blocks" in a blockchain
 * protocol such as Bitcoin.  Besides the crypto miner problem type, we include a
 * "trivial" problem type, which requires no actual work to solve and can be used
 * for testing purposes.  The "null problem type" has no associated solver, and
 * is just used as a mechanism to cause worker processes to terminate gracefully.
 * However, there is no reason why we couldn't add additional problem types, such
 * as a "checkmate problem" type, in which worker processes search for forced
 * checkmates from a given chess position, or any other similar type of problem
 * for that matter.
 */
#define NULL_PROBLEM_TYPE         0
#define TRIVIAL_PROBLEM_TYPE      1
#define CRYPTO_MINER_PROBLEM_TYPE 2

#define NUM_PROBLEM_TYPES         3

/*
 * Format of a "problem".
 * A problem consists of a fixed-size header (struct problem), followed by
 * an arbitrary number of bytes of problem-specific data.  The header specifies
 * the total size of the problem, which is the total of the size of the header
 * and the problem data, an integer code that specifies the type of problem to
 * be solved (which determines how the data is to be interpreted), and a number
 * of possible variant forms that can be created of the same problem.
 *
 * The idea of variant forms for a problem is that they represent alternative
 * ways of presenting the same underlying problem, so that the solution of any
 * one of the variant forms will be considered as a solution for the underlying
 * problem.  The variant forms of a problem differ in some way that would make it
 * useful for several processes to try to solve variant forms of the same problem
 * concurrently.  For example, each variant form of a search problem might include
 * a different starting point for the search, so that processes carrying out
 * these searches will not be doing redundant work.  The number of variant forms
 * of a problem is set when a problem instance is created, and typically this
 * would be equal to the number of workers that might be trying to solve the
 * problem concurrently.
 */
struct problem {
    size_t size;     // Total length in bytes, including size and type.
    short type;      // Problem type.
    short id;        // Problem ID.
    short nvars;     // Number of possible variant forms of the problem.
    short var;       // The variant of the problem that this is.
    char padding[0]; // To align the subsequent data on a 16-byte boundary.
    char data[0];    // Data for the problem (depends on problem type).
};

/*
 * Format of a "result".
 * The format of a result is similar to that of a problem, except that
 * the header of a result (struct result) contains a field that is nonzero if
 * the attempt to solve the problem failed.  If the solution attempt succeeded,
 * then this field is zero and the data portion of the result contains the actual
 * solution.
 */
struct result {
    size_t size;     // Total length in bytes, including size.
    short id;        // Problem ID.
    char failed;     // Nonzero if the solution attempt failed or was canceled.
    char padding[5]; // To align the subsequent data on a 16-byte boundary.
    char data[0];    // Data for the result (depends on problem type).
};

/*
 * Each problem type has an associated "solver", which provides four methods
 * for manipulating problems of that type.  These methods are:
 *
 *   1.  A "constructor", which is used to construct a problem given some parameters.
 *   2.  A "varier", which modifies a problem to one of a number of variant forms.
 *   3.  A "solver", which accepts a problem and returns a result.
 *   4.  A "checker", which checks whether a specified result solves a specified problem.
 *
 * These are specified in more detail below.
 */

/*
 * "Constructor"
 *
 * @brief  Constructs a problem, given some parameters.
 * @param  (various) Parameters that depend on the particular problem type.
 * @return  A problem constructed using the supplied parameters, or NULL if for
 * some reason the problem could not be constructed as specified.  The returned
 * problem structure is created by malloc, and it is the caller's responsibility
 * to free it when it is no longer needed.
 */
typedef struct problem *(CONSTRUCTOR)();

/*
 * "Varier"
 *
 * @brief Modifies a problem to one of a number of variant forms.
 * @param prob  The problem to be varied.
 * @param var  The variant form to be created.  This must be in the range [0, nvars),
 * where nvars is the number of possible variant forms recorded in the specified problem.
 * @modifies  The structure pointed at by prob, to a variant form of the same underlying
 * problem.
 */
typedef void (VARIER)(struct problem *prob, int var);

/*
 * "Solver"
 *
 * @brief Attempts to solve a problem and return a result.
 * @param prob  The problem to be solved.
 * @param canceledp  Pointer to a flag that will be set if the current solution attempt
 * is to be canceled.  The solver is responsible for checking this flag periodically and
 * returning in case of cancellation.
 * @return  A result structure, if the solver terminated normally, otherwise NULL if the
 * solver was canceled or otherwise failed.  The result structure is created by malloc,
 * and it is the caller's responsibility to free it when it is no longer needed.
 */
typedef struct result *(SOLVER)(struct problem *prob, volatile sig_atomic_t *canceledp);

/*
 * "Checker"
 *
 * @brief Checks whether a specified result solves a specified problem.
 * @param result  The result to be checked.
 * @param prob  The problem that the result supposedly solves.
 * @return 0 if the result is not marked as "failed" and it does indeed
 * solve the specified problem.  Nonzero if the result is marked "failed"
 * or it fails to solve the problem.
 */
typedef int (CHECKER)(struct result *result, struct problem *prob);

/*
 * Structure containing "methods" provided by a problem solver.
 * The solver for each problem type provides such a structure.
 * It is possible for the fields of this structure to be NULL.
 */
struct solver_methods {
    CONSTRUCTOR *construct;
    VARIER *vary;
    SOLVER *solve;
    CHECKER *check;
};

/*
 * The following table maps problem types to the corresponding solver functions.
 * It is used to invoke the proper solver for each problem.
 */
extern struct solver_methods solvers[NUM_PROBLEM_TYPES];

/*
 * init_problems
 *
 * @brief Function to initialize solvers and to populate the solvers table with methods
 * for the known solvers.
 * @details This must be called before any attempt is made to invoke solver methods.
 * @param nprobs  The total number of problems to be generated.
 * @param mask  Bit mask that has a 1 in bit i if the solver for problem
 * type i is to be enabled.
 */
void init_problems(int nprobs, unsigned int mask);

/*
 * get_problem_variant
 *
 * @brief Create and return a specified variant of a problem.
 * @details  If there is a current problem, a variant of that is created.
 * If there is no current problem, then a new problem is created.
 * @param nvars  The number of possible variant forms of the problem.
 * @param var  The particular variant to be created.
 * @return  A pointer to the problem variant, or NULL if there are no more
 * problems.  The returned pointer is only valid until the next call to
 * get_problem_variant, after which it should not be used.  The caller
 * must not attempt to free the pointer that is returned.
 */
struct problem *get_problem_variant(int nvars, int var);

/*
 * post_result
 *
 * @brief Post a result obtained by an attempt to solve a problem.
 * @details It is checked whether the result actually does solve the specified
 * problem.  If so, then the problem in question is marked as "solved" and no
 * further variants of this problem will be returned by get_problem_variant().
 *
 * @param result  The result to be posted.
 * @param prob  The problem that the result supposedly solves.
 * @return 0 if the result is not marked "failed" and it is indeed a solution
 * to the specified problem, otherwise nonzero is returned.
 */
int post_result(struct result *result, struct problem *prob);

/* Maximum supported number of worker processes. */
#define MAX_WORKERS 32

/*
 * Workers cycle between the following states:
 *   started: worker has just started
 *   idle: worker is stopped, no result to be read
 *   continued: worker has been continued, not yet observed to be active
 *   running: worker is running, solving a problem
 *   stopped: worker is stopped, result waiting to be read
 *   exited: worker terminated normally
 *   aborted: worker terminated abnormally
 */
#define WORKER_STARTED 1
#define WORKER_IDLE 2
#define WORKER_CONTINUED 3
#define WORKER_RUNNING 4
#define WORKER_STOPPED 5
#define WORKER_EXITED 6
#define WORKER_ABORTED 7

/*
 * EVENT FUNCTIONS THAT YOU MUST CALL FROM WITHIN YOUR MASTER PROCESS
 * See the assignment document for further information.
 */

/* To be called when the master process has just begun to execute. */
void sf_start(void);

/* To be called when the master process is about to terminate. */
void sf_end(void);

/*
 * To be called when the master process causes or observes a worker process
 * to change state.
 *
 * @param pid  The process ID of the worker process that has changed state.
 * @param oldstate  The state that the worker process was in before the change.
 * @param newstate  The state that the worker process is in after the change.
 */
void sf_change_state(int pid, int oldstate, int newstate);

/*
 * To be called when the master process sends a problem over the pipe to
 * a worker process.
 *
 * @param pid  The process ID of the worker process to which the problem is sent.
 * @param prob  The problem structure that is being sent.
 */
void sf_send_problem(int pid, struct problem *prob);

/*
 * To be called when the master process has received a result over the pipe
 * from a worker process.
 *
 * @param pid  The process ID of the worker process from which the result has
 * been received.
 * @param result  The result structure that was received.
 */
void sf_recv_result(int pid, struct result *result);

/*
 * To be called when the master process is about to notify a worker process that
 * the current solution attempt is to be canceled.
 *
 * @param pid  The process ID of the worker process that is being notified.
 */
void sf_cancel(int pid);

/*
 * FUNCTIONS YOU ARE TO IMPLEMENT
 * For further information, refer to the homework document.
 */

/*
 * master
 *
 * @brief  Main function for the master process.
 * @details  Performs required initialization, creates a specified number of
 * worker processes and enters a main loop that repeatedly assigns problems
 * to workers and posts results received from workers, until all of the worker
 * processes are idle and a NULL return from get_problem_variant indicates that
 * there are no further problems to be solved.  Once this occurs, each worker
 * process is sent a SIGTERM signal, which they should handle by terminating
 * normally.  When all worker processes have terminated, the master process
 * itself terminates.
 * @param workers  If positive, then the number of worker processes to create.
 * Otherwise, the default of one worker process is used.
 * @return  A value to be returned as the exit status of the master process.
 * This value should be EXIT_SUCCESS if all worker processes have terminated
 * normally, otherwise EXIT_FAILURE.
 */
int master(int workers);

/*
 * worker
 *
 * @brief  Main function for a worker process.
 * @details  Performs required initialization, then enters a main loop that
 * repeatedly receives from the master process a problem to be solved, attempts
 * to solve that problem, and returns the result of the solution attempt to
 * the master process.  Signals SIGSTOP and SIGCONT are used to stop and start
 * the worker process as described in the homework document.
 * If a SIGHUP signal is received by a worker while it is trying to solve a problem,
 * the worker process will abandon the current solution attempt, send a "failed" result
 * back to the master process, and stop, awaiting a new problem to be sent by the master.
 * If a SIGTERM signal is received by a worker, it will terminate normally with
 * exit status EXIT_SUCCESS.
 * @return EXIT_SUCCESS upon receipt of SIGTERM.
 */
int worker(void);

#endif
