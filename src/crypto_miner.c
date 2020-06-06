/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototypes or constants in a header, please put them
 * in another header file.
 *
 * When we grade, we will be replacing this file with our own copy.
 * You have been warned.
 * === DO NOT MODIFY THIS FILE ===
 */

#include <stdio.h>
#include <unistd.h>
#include <gcrypt.h>

#include "debug.h"
#include "polya.h"

/*
 * Format of a crypto miner problem.
 * This specializes the generic problem format defined in polya.h.
 */
struct crypto_miner_problem {
    size_t size;        // Total length in bytes, including size and type.
    short type;         // Problem type.
    short id;           // Problem ID.
    short nvars;        // Number of possible variant forms of the problem.
    short var;          // This variant of the problem.
    char padding[0];    // To align the subsequent data on a 16-byte boundary.
    int bsize;          // Size of the block, in bytes.
    int nsize;          // Size of a nonce, in bytes.
    short diff;         // Difficulty level to be satisfied.
    char data[0];       // Data: block, followed by starting nonce.
};

/*
 * Format of a crypto miner solution.
 * This specializes the generic solution format defined in polya.h.
 */
struct crypto_miner_result {
    size_t size;     // Total length in bytes, including size.
    short id;        // Problem ID.
    char failed;     // Whether the solution attempt failed.
    char padding[5]; // To align the subsequent data on a 16-byte boundary.
    int nsize;       // Size of a nonce, in bytes.
    char nonce[0];   // Nonce that solves the problem.
};

/*
 * Code used by a worker to solve a "crypto miner problem".
 */

static struct problem *crypto_miner_construct_problem
	(int nvars, int id, char *block, size_t bsize, size_t nsize, int diff);
static void crypto_miner_vary_problem(struct problem *aprob, int var);
static struct result *crypto_miner_solver(struct problem *aprob, volatile sig_atomic_t *canceledp);
static int crypto_miner_check_result(struct result *aresult, struct problem *aprob);

static int solve(char *block, size_t bsize,
		 unsigned char *nonce, size_t nsize, unsigned int diff,
		 volatile sig_atomic_t *cancelp);
static int check_result(unsigned char *digest, size_t dsize, unsigned int diff);
static void init_nonce(unsigned char *nonce, size_t nsize);
static int update_nonce(unsigned char *nonce, size_t nsize);

/*
 * Initialize the crypto_miner solver.
 */
struct solver_methods crypto_miner_solver_methods = {
   crypto_miner_construct_problem, crypto_miner_vary_problem, crypto_miner_solver,
   crypto_miner_check_result
};

void crypto_miner_solver_init(void) {
    solvers[CRYPTO_MINER_PROBLEM_TYPE] = crypto_miner_solver_methods;
}

/*
 * Create a "crypto miner problem" from given parameters.
 * Returns a pointer to the constructed problem.  Caller must free.
 *
 * @param id     The problem ID.
 * @param nvars  The number of possible variants of the problem.
 * @param block  Data comprising the block to be solved.
 * @param bsize  Size of the block data.
 * @param nsize  Size of the nonce.
 * @param diff   Difficulty level.
 * @return  A pointer to the problem that was created, if creation was successful,
 * otherwise NULL.  The caller is responsible for freeing any non-NULL pointer
 * that is returned.
 */
static struct problem *crypto_miner_construct_problem
	(int id, int nvars, char *block, size_t bsize, size_t nsize, int diff) {
    struct crypto_miner_problem *prob = NULL;
    size_t size = sizeof(*prob) + bsize + nsize;
    prob = malloc(size);
    if(prob == NULL)
	return NULL;
    memset(prob, 0, size);
    prob->size = size;
    prob->type = CRYPTO_MINER_PROBLEM_TYPE;
    prob->nvars = nvars;
    prob->id = id;
    prob->bsize = bsize;
    prob->nsize = nsize;
    // Random difficulty 20 to diff.
    prob->diff = diff <= 20 ? 20 : 20 + random() % (diff - 20 + 1);
    // Copy the block data into the problem.
    memcpy(prob->data, block, bsize);
    return (struct problem *)prob;
}

/*
 * Modify a given problem to create one of a number of variant forms.
 * A solution to any of the variants is considered as a solution to the given problem. 
 *
 * @param aprob  The problem to be modified.
 * @param var  Integer in the range [0, aprob->nvars) specifying the particular variant
 * form to be created.
 * @modifies aprob  to be the specified variant form.
 */
static void crypto_miner_vary_problem(struct problem *aprob, int var) {
    struct crypto_miner_problem *prob = (struct crypto_miner_problem *)aprob;
    // Initialize the starting nonce according to the specified variant.
    // Specifically, we clear the nonce and initialize the most-significant byte
    // to a value associated with the desired variant.
    //
    // The idea here is that "nvars" might be a number of processes that will be
    // searching concurrently for a solution, and "var" might be the process ID of
    // the particular process that will be given this variant.  The variants are
    // constructed so that the starting nonces used by the processes will be
    // spaced evenly over the space of all possible nonces.  This keeps the
    // searches performed by the processes from overlapping.
    memset(prob->data + prob->bsize, 0, prob->nsize);
    if(prob->nvars) {
	prob->data[prob->bsize + prob->nsize - 1] = ((var * 256) / prob->nvars) & 0xff;
	prob->var = var;
    }
}

/*
 * Solve a "crypto miner problem", returning the solution if successful.
 *
 * @param aprob  Pointer to a structure describing the problem to be solved.
 * @param canceledp  Pointer to a flag indicating whether the current solution attempt
 * is to be canceled.
 * @return If the problem was successfully solved, then the returned value will be
 * a pointer to a result object whose "failed" field is 0 and whose "data" field contains
 * a description of the problem solution (in this case, the nonce that satisfied the
 * difficulty requirement).  A return of a non-NULL pointer, but a nonzero value in the
 * "failed" field of the pointed-at result object indicates that the solver terminated
 * normally, but failed to solve the problem.  Otherwise, return of a NULL pointer
 * means that either allocation of the result object failed or the solver was interrupted
 * before this allocation occurred.  In all cases, the caller is responsible for freeing
 * any non-NULL pointer that is returned.
 */
static struct result *crypto_miner_solver(struct problem *aprob, volatile sig_atomic_t *canceledp) {
    struct crypto_miner_problem *prob = (struct crypto_miner_problem *)aprob;
    int failed;
    debug("[%d:Worker] Crypto miner solver (id = %d, bsize = %d, nsize = %d, diff = %d)",
	  getpid(), prob->id, prob->bsize, prob->nsize, prob->diff);
    unsigned char *nonce = malloc(prob->nsize);
    // Copy initial nonce from the problem data.
    memcpy(nonce, prob->data + prob->bsize, prob->nsize);
    switch(failed = solve(prob->data, prob->bsize, nonce, prob->nsize, prob->diff, canceledp)) {
    case -1:
	// Solving was canceled.
	free(nonce);
	return NULL;
    case 1:
	// The space of possible nonce values was exhausted without finding a solution.
	free(nonce);
	return NULL;
    case 0:
	{
	    size_t size = sizeof(struct crypto_miner_result) + prob->nsize;
	    struct crypto_miner_result *result = malloc(size);
	    if(result == NULL)
		return NULL;
	    memset(result, 0, size);
	    result->size = size;
	    result->id = prob->id;
	    result->failed = failed;
	    result->nsize = prob->nsize;
	    memcpy(result->nonce, nonce, prob->nsize);
	    free(nonce);
	    debug("[%d:Worker] Returning result (nsize = %d)", getpid(), result->nsize);
	    return (struct result *)result;
	}
    default:
	debug("[%d:Worker] Unexpected return value from solve()", getpid());
	abort();
    }
}

/*
 * Check whether a specified "result" solves a specified "problem".
 *
 * @param aresult  The result to be checked.
 * @param aprob  The problem the result is supposed to solve.
 * @return  0 if the result is not marked "failed" and it does indeed solve the problem;
 * nonzero if the result is marked "failed" or it fails to solve the problem.
 */
static int crypto_miner_check_result(struct result *aresult, struct problem *aprob) {
    struct crypto_miner_problem *prob = (struct crypto_miner_problem *)aprob;
    struct crypto_miner_result *result = (struct crypto_miner_result *)aresult;

    // If the result is marked "failed" it is not a solution.
    if(result->failed) {
	debug("[%d] Result is marked 'failed'", getpid());
	return -1;
    }

    // Otherwise, hash the concatenation of the block from the problem and
    // the nonce from the solution and check whether the result satisfies the
    // difficulty specification.
    gcry_md_hd_t h;
    unsigned char *x;
    size_t dsize;
    dsize = gcry_md_get_algo_dlen(GCRY_MD_SHA256);  // get the digest length
    gcry_md_open(&h, GCRY_MD_SHA256, GCRY_MD_FLAG_SECURE);
    if(h == NULL) {
	debug("[%d] Check result failed in libgcrypt", getpid());
	return -1;
    }
    gcry_md_write(h, prob->data, prob->bsize);      // hash the block
    gcry_md_write(h, result->nonce, result->nsize); // hash the nonce
    x = gcry_md_read(h, GCRY_MD_SHA256);            // get the result
    if(check_result(x, dsize, prob->diff)) {
	gcry_md_close(h);
	return 0;
    } else {
	gcry_md_close(h);
	return 1;
    }
}

/*
 * This function attempts to "solve" a block by iterating through a space of nonces.
 * For each nonce, the concatenation of the block and the nonce is hashed, and the resulting
 * digest is checked to see if it has the characteristics required of a solution.
 *
 * @param block  Pointer to the block to be solved.
 * @param bsize  Size of the block in bytes.
 * @param nonce  Pointer to an area where the nonce is to be stored.
 * @param nsize  Size of the nonce in bytes.
 * @param diff  The "difficulty" to be satisfied.  A digest satisfies the difficulty if
 * it has this many leading zero bits.
 * @param canceledp  Pointer to a flag which, if set, indicates that the current solution attempt
 * should be abandoned.
 * @return 0 if a solution is found, 1 if the space of possible nonces is exhausted without
 * finding any solution, -1 if solving was canceled.
 */
static int solve(char *block, size_t bsize,
		 unsigned char *nonce, size_t nsize, unsigned int diff,
		 volatile sig_atomic_t *canceledp)
{
    long iter = 0;
    unsigned char *x;
    size_t dsize;
    gcry_md_hd_t h;
    dsize = gcry_md_get_algo_dlen(GCRY_MD_SHA256);  // get the digest length
    gcry_md_open(&h, GCRY_MD_SHA256, GCRY_MD_FLAG_SECURE);
    if(h == NULL) {
	debug("[%d:Worker] gcry_md_open failed", getpid());
	abort();
    }
    do {
	if(*canceledp) {
	    debug("[%d:Worker] Crypto miner solver canceled", getpid());
	    gcry_md_close(h);
	    return -1;
	}
	iter++;
	gcry_md_write(h, block, bsize); // hash the block
	gcry_md_write(h, nonce, nsize); // hash the nonce
	x = gcry_md_read(h, GCRY_MD_SHA256); // get the result
	if(check_result(x, dsize, diff)) {
	    char hex[] = { '0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
	    char *buf;
	    debug("[%d:Worker] Solution found at iteration %lu", getpid(), iter);
	    buf = malloc(2*dsize+1);
	    for(int i = 0; i < dsize; i++) {
		buf[2*i] = hex[x[i] & 0xf];
		buf[2*i+1] = hex[(x[i] >> 4) & 0xf];
	    }
	    buf[2*dsize] = '\0';
	    debug("[%d:Worker] %s", getpid(), buf);
	    free(buf);
	    gcry_md_close(h);
	    return 0;
	}
	gcry_md_reset(h);
    } while(update_nonce(nonce, nsize));
    debug("[%d:Worker] No solution found after %lu iterations", getpid(), iter);
    return 1;
}

/*
 * Check whether a digest satisfies a specified "difficulty" requirement.
 *
 * @param digest  Pointer to the digest to be checked.
 * @param dsize  Size of the digest.
 * @param diff  Number of leading zero bits that must be in the digest.
 * @return nonzero if the digest has the specified number of leading zero bits,
 * 0 otherwise.
 */
static int check_result(unsigned char *digest, size_t dsize, unsigned int diff)
{
    for(int i = 0; i < dsize; i++) {
	for(unsigned char mask = 0x80; mask != 0; mask >>= 1) {
	    if(digest[i] & mask)
		return 0;
	    if(--diff == 0)
		return 1;
	}
    }
    debug("[%d:Worker] Difficulty (%d) too large for digest size (%lu)",
	  getpid(), diff, dsize);
    return 0;
}

/*
 * Update a nonce to the next possible value.
 * The bytes of a nonce are treated as a base-256 counter, least-significant digit
 * first.  The counter is updated by incrementing the first byte and propagating
 * any "carry" to the subsequent bytes.
 *
 * @param nonce  Pointer to the nonce to be updated.
 * @param nsize  Size of the nonce (must be nonzero).
 * @return nonzero if the nonce was successfully updated, 0 if the nonce value space
 * was exhausted and it was not possible to update the nonce.
 */
static int update_nonce(unsigned char *nonce, size_t nsize)
{
    int carry = 1;
    size_t i = 0;
    while(carry && i < nsize)
	carry = !++nonce[i++];
    return(!(carry && i == nsize));
}

