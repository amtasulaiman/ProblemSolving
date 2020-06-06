#include <stdlib.h>

#include "debug.h"
#include "polya.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>


volatile sig_atomic_t sig_chld = 0;

volatile sig_atomic_t sig_pipe = 0;

int *process_ids;
int num_of_tasks;
int *process_state;

void sig_pipe_handler(int signum) {
  sig_pipe = 1;
}

void sig_chld_handler(int signum) {
    sig_chld = 1;
    int status;
    pid_t c_pid ;
    while((c_pid = waitpid(-1, &status, WUNTRACED | WCONTINUED | WNOHANG)) > 0){
        for(int i=0; i<num_of_tasks; i++){
            if(c_pid != process_ids[i]){

                if (WIFCONTINUED(status)) {
                    sf_change_state(c_pid, process_state[i], WORKER_RUNNING);
                    process_state[i] = WORKER_RUNNING;
                }

                else if (WIFSTOPPED(status)) {
                    if(process_state[i] != WORKER_STARTED){
                        sf_change_state(c_pid, process_state[i], WORKER_STOPPED);
                        process_state[i] = WORKER_STOPPED;
                    }
                    else{
                        sf_change_state(c_pid, process_state[i], WORKER_IDLE);
                        process_state[i] = WORKER_IDLE;
                    }
                }
                else if (WIFEXITED(status)) {
                    sf_change_state(c_pid, process_state[i], WORKER_EXITED);
                    process_state[i] = WORKER_EXITED;
                }
            }
        }

    }
    exit(EXIT_SUCCESS);
}
/*
 * master
 * (See polya.h for specification.)
 */
int master(int workers) {

    sf_start();
    pid_t pid;
    int master_p[workers][2];
    int worker_p[workers][2];
    int all_problems_solved = 0;
    int idle_states = 0;
    num_of_tasks = workers;
    process_state = malloc(sizeof(int)*workers);

    process_ids = malloc(sizeof(int)*workers);

    signal(SIGPIPE,sig_pipe_handler);
    signal(SIGCHLD,sig_chld_handler);

    sigset_t mask;
    int sig_empty_result = sigemptyset(&mask);
    int sig_add_result = sigaddset(&mask,SIGCHLD);

     sigset_t empty;
     if(sig_empty_result == -1 || sig_add_result == -1)
     {
         exit(EXIT_FAILURE);
     }

    //loop for # of tasks
    for(int i = 0; i < num_of_tasks; i++){


        if(pipe(master_p[i]) == -1){
            exit(EXIT_FAILURE);
        }
        if(pipe(worker_p[i]) == -1){
            exit(EXIT_FAILURE);
        }

        pid = fork();
        if(pid == -1){
            exit(EXIT_FAILURE);
        }
        else if(pid > 0){ //Parent process
            sigprocmask(SIG_BLOCK,&mask,&empty);
            process_ids[i] = pid;
            sf_change_state(pid, 0, WORKER_STARTED);
            process_state[i] = WORKER_STARTED;

            sigprocmask(SIG_SETMASK,&empty,NULL);

        } //end if
        //Child process
        else if (pid ==  0){
            //creating pipes
            dup2(worker_p[i][1], fileno(stdout));
            close(worker_p[i][0]);
            dup2(master_p[i][0], fileno(stdin));
            close(master_p[i][1]);

            execl("bin/polya_worker", "bin/polya_worker", NULL);

        }//end else if
    }//for loop ends
        struct problem *probl;

        for(;;){
            //loop to assign problem to idle workers
            for(int i=0; i<num_of_tasks; i++){
                if(process_state[i] == WORKER_IDLE){

                    probl = get_problem_variant(num_of_tasks, i);
                    if(probl == NULL){
                        debug("No problem is left");
                        all_problems_solved++;
                        kill(process_ids[i], SIGTERM);
                    }
                    else{
                        FILE *wfile;
                        wfile = fdopen(master_p[i][fileno(stdout)], "w");
                        if(wfile != NULL){
                            fwrite(probl,probl->size, 1, wfile);
                            fflush(stdout);
                            sf_send_problem(pid, probl);

                            sigprocmask(SIG_BLOCK,&mask,&empty);
                            sf_change_state(process_ids[i], process_state[i], WORKER_CONTINUED);
                            process_state[i] = WORKER_CONTINUED;
                            sigprocmask(SIG_SETMASK,&empty,NULL);
                        }
                    }
                 }

                if(process_state[i] == WORKER_RUNNING){

                    sigprocmask(SIG_BLOCK,&mask,&empty);
                    kill(pid, SIGHUP);
                    sf_change_state(process_ids[i], process_state[i], WORKER_STOPPED);
                    process_state[i] = WORKER_STOPPED;
                    sigprocmask(SIG_SETMASK,&empty,NULL);

                }
            }
            int i = 0;
            //loop to retrieve result from stopped workers
            while(i<num_of_tasks){

                if(process_state[i] == WORKER_STOPPED){

                    struct result *answer = malloc(sizeof(struct result));
                    sf_recv_result(process_ids[i], answer);
                    FILE *rfile;
                     rfile = fdopen(worker_p[i][fileno(stdin)], "r");
                    if(rfile != NULL){
                        fread(answer, sizeof(struct result), 1, rfile);

                        answer = realloc(answer,answer->size);
                        fread(answer->data, (answer->size) - sizeof(struct result), 1, rfile);
                        sf_recv_result(process_ids[i], answer);

                        sigprocmask(SIG_BLOCK,&mask,&empty);
                        sf_change_state(process_ids[i],process_state[i], WORKER_IDLE);
                        process_state[i] = WORKER_IDLE;
                        sigprocmask(SIG_SETMASK,&empty,NULL);

                    }
                    else{
                        exit(EXIT_FAILURE);
                    }
                    //checking if answer is correct
                    if(post_result(answer, probl)==0){
                        debug("Result is Correct!");
                       for(int j=0; j<num_of_tasks;j++){
                           if(process_state[j] == WORKER_CONTINUED || process_state[j] == WORKER_RUNNING){

                                sf_cancel(process_ids[j]);
                                sigprocmask(SIG_BLOCK,&mask,&empty);
                                kill(process_ids[j], SIGHUP);
                                sf_change_state(process_ids[j],process_state[j], WORKER_IDLE);
                                process_state[j] = WORKER_IDLE;
                                sigprocmask(SIG_SETMASK,&empty,NULL);
                           }
                       }
                    }

                    free(answer);
                }
                i++;
            }//task completion for loop
            for(int k=0; k<num_of_tasks;k++){
                if(process_state[k]==WORKER_IDLE){
                    idle_states++;
                }
            }
            if(all_problems_solved==1 && idle_states==num_of_tasks){

                for(int i=0; i<num_of_tasks; i++){
                    kill(process_ids[i], SIGTERM);
                    kill(process_ids[i], SIGCONT);
                    if(process_state[i]==WORKER_EXITED)
                        pause();
                }
                free(probl);
                free(process_ids);
                free(process_state);
                sf_end();
                exit(EXIT_SUCCESS);
            }

    }//endless loop ends

    sf_end();
    return EXIT_FAILURE;
}

