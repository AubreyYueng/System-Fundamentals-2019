/*
 * Job manager for "jobber".
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include "jobber.h"
#include "task.h"

typedef struct JOB {
    JOB_STATUS job_status;
    int job_id;
    pid_t pgid;
    TASK* task;
    int exit_status;
    pid_t* cpids;
    int cmd_total;
    int canceled;   // cancel by user or signal
} JOB;

JOB job_table[MAX_JOBS];
// char spec[1024];
int enable = 0;

void check_and_execute_jobs();
void execute_job(JOB* job);
void close_fd(int fd[2]);
int count_cmd_in_pipeline(PIPELINE* pipeline);
int count_cmd_in_task(TASK* task);
void execute_command(COMMAND* command);
void execute_pipeline(PIPELINE* pl, pid_t cpids[], int cmd_st_idx, pid_t* pgid_ptr, JOB* job);
void verify_fd(int in, char* path);
void close_f(int fd, char* path);
int check_a_job(JOB* job);
int print_exit(char* err_msg);
void alter_status(JOB* job, JOB_STATUS job_status);
int job_cancel_internal(int jobid, int need_err_msg);
int job_expunge_internal(int jobid, int need_err_msg);
int process_wstatus(int wstatus, JOB* job);
void terminate_handler(int sig);

int jobs_init(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        job_table[i].job_id = i;
        job_table[i].task = NULL;
        job_table[i].exit_status = 0;
    }

    struct sigaction action;
    action.sa_handler = terminate_handler;
    action.sa_flags = 0;
    sigaction (SIGCHLD, &action, NULL);
    
    return 0;
}

void jobs_fini(void) {
    debug("into finish...");
    signal(SIGCHLD, SIG_DFL);
    for (int i = 0; i < MAX_JOBS; i++) {
        job_cancel_internal(i, 0);
    }
    for (int i = 0; i < MAX_JOBS; i++)
    {
        JOB* job = &job_table[i];
        if (job->task == NULL) continue;
        JOB_STATUS job_status = job->job_status;
        if (job_status==RUNNING||job_status==CANCELED||job_status==PAUSED) {
            int wstatus = 0;
            int result;
            pid_t pgid = job->pgid;
            debug("starts to wait for the job%d, pgid %d", job->job_id, pgid);
            result = waitpid (pgid, &wstatus, 0);
            if (result != pgid) {
                debug("waitpid result of %d not equals to pgid %d, %d--%s", result, pgid, errno, strerror(errno));
            } else {
                debug("waitpid result of %d equals to pgid %d, %d--%s", result, pgid, errno, strerror(errno));
            }
            process_wstatus(wstatus, job);
        }
        job_expunge_internal(i, 0);
    }
}

int jobs_set_enabled(int val) {
    enable = val;
    check_and_execute_jobs();
    return 0;
}

int jobs_get_enabled() {
    return enable;
}

int job_create(char *command) {
    for (int i = 0; i < MAX_JOBS; i++)
    {
        JOB* job = &job_table[i];
        if (job->task == NULL)
        {
            job->job_id = i;
            job->job_status = NEW;
            job->pgid = -1;
            job->canceled = 0;

            TASK* task = parse_task(&command);
            job->task = task;
            sf_job_create(i);

            sf_job_status_change(i, NEW, WAITING);
            job->job_status = WAITING;

            check_and_execute_jobs();
            return 0;
        }
    }
    debug("Maybe jobs amount over limit");
    return -1;
}

int job_expunge(int jobid) {
    return job_expunge_internal(jobid, 1);
}

int job_expunge_internal(int jobid, int need_err_msg) {
    char* err_msg = "Error: expunge";
    if (jobid >= MAX_JOBS || jobid < 0) {
        debug("invalid jobid");
        return need_err_msg==1 ? print_exit(err_msg) : -1;
    }
    
    JOB* job = &job_table[jobid];
    if (job->task == NULL || (job->job_status!=ABORTED && job->job_status!=COMPLETED)) 
        return need_err_msg==1 ? print_exit(err_msg) : -1;

    free_task(job->task);
    job->task = NULL;
    sf_job_expunge(jobid);
    return 0;
}

// If the specified job ID is not the ID of an existing job, or the job has already terminated, 
// then the job cannot be canceled
int job_cancel(int jobid) {
    return job_cancel_internal(jobid, 1);
}

int job_cancel_internal(int jobid, int need_err_msg) {
    char* err_msg = "Error: cancel";
    if (jobid >= MAX_JOBS || jobid < 0) return need_err_msg==1 ? print_exit(err_msg) : -1;

    JOB* job = &job_table[jobid];
    if (job->task == NULL || job->job_status==CANCELED || job->job_status==ABORTED || job->job_status==COMPLETED) 
        return need_err_msg==1 ? print_exit(err_msg) : -1;
    
    if(killpg(job->pgid, SIGKILL) == -1) {
        debug("error occurs while killing job %d, %d--%s", jobid, errno, strerror(errno));
        return need_err_msg==1 ? print_exit(err_msg) : -1;
    }
    
    job->canceled = 1;
    if (job->job_status==NEW || job->job_status==WAITING)
        alter_status(job, ABORTED);
    else
        alter_status(job, CANCELED);
    
    return 0;
}

int job_pause(int jobid) {
    char* err_msg = "Error: pause";
    if (jobid >= MAX_JOBS || jobid < 0) return print_exit(err_msg);
    
    JOB* job = &job_table[jobid];
    if (job->task == NULL || job->job_status!=RUNNING) {
        debug("pause failed, job %d not running", jobid);
        return print_exit(err_msg);
    }

    if(killpg(job->pgid, SIGSTOP) < 0) {
        debug("error occurs while pausing job %d, pgid %d, %d--%s", jobid, job->pgid, errno, strerror(errno));
        return print_exit(err_msg);
    } else {
        debug("pause signal sent to job %d, pgid: %d", jobid, job->pgid);
    }
    return 0;
}

int job_resume(int jobid) {
    char* err_msg = "Error: resume";
    if (jobid >= MAX_JOBS || jobid < 0) return print_exit(err_msg);

    JOB* job = &job_table[jobid];
    if (job->task == NULL) return -1;
    if (job->job_status!=PAUSED) {
        debug("job_status for %d is not PAUSED", jobid);
        return print_exit(err_msg);
    }

    debug("pgid for the resumed job is %d", job->pgid);
    if(killpg(job->pgid, SIGCONT) == -1) {
        debug("error occurs while pausing job %d, %d--%s", jobid, errno, strerror(errno));
        return print_exit(err_msg);
    }
    
    return 0;
}

int job_get_pgid(int jobid) {
    if (jobid >= MAX_JOBS || jobid < 0) return -1;
    
    JOB* job = &job_table[jobid];
    if (job->task == NULL) return -1;
    JOB_STATUS job_status = job->job_status;
    return job_status == CANCELED || job_status == RUNNING || job_status == PAUSED ? job->pgid : -1;
}

JOB_STATUS job_get_status(int jobid) {
    if (jobid >= MAX_JOBS || jobid < 0) return -1;
    
    JOB* job = &job_table[jobid];
    if (job->task == NULL) return -1;
    return job->job_status;
}

int job_get_result(int jobid) {
    if (jobid >= MAX_JOBS || jobid < 0) return -1;
    JOB* job = &job_table[jobid];
    if (job->task == NULL) return -1;

    return job->exit_status;
}

int job_was_canceled(int jobid) {
    if (jobid >= MAX_JOBS || jobid < 0) return -1;
    JOB* job = &job_table[jobid];
    if (job->task == NULL) return -1;

    return job->canceled;
}

char *job_get_taskspec(int jobid) {
    if (jobid >= MAX_JOBS || jobid < 0) return NULL;
    JOB* job = &job_table[jobid];
    if (job->task == NULL) return NULL;

    char spe[1024];
    char* spec = spe;
    FILE *out = fmemopen(spec, 1024, "w+");
    unparse_task(job->task, out);

    fgets(spec, 1024, out) ;
    fclose(out);

    return spec;
}

/*--------------------  self defined functions --------------------*/
void check_and_execute_jobs() {
    if (!enable) {
        debug("enable flag is %d", enable);
        return;
    }

    int running_cnt = 0;
    for (int i = 0; i < MAX_JOBS; i++)
    {
        JOB* job = &job_table[i];
        JOB_STATUS job_status = job->job_status;
        if (job->task != NULL && running_cnt<MAX_RUNNERS) {
            if (WAITING==job_status) {
                debug ("in dealing execute job pid %d", getpid()) ;
                execute_job(job);
                running_cnt++;
            } 
            else if (RUNNING==job_status || PAUSED==job_status) 
                running_cnt++;
        }
    }
}

void execute_job(JOB* job) {
    debug("start to execute job %d", job->job_id);
    int cmd_total = count_cmd_in_task(job->task);
    pid_t cpids[cmd_total];
    job->cpids = cpids;
    job->cmd_total = cmd_total;

    int pl_idx = 0;
    int cmd_st_idx = 0;     // cmd index at the beginning of each pipeline
    pid_t pgid = 0;

    PIPELINE_LIST* curr = job->task->pipelines;
    alter_status(job, RUNNING);
    pid_t cpid = fork();
    if (cpid == 0) {
        signal(SIGCHLD, SIG_DFL);
        while (curr)
        {
            if (pgid == 0) {
                setpgrp(); 
                pgid = getpid();
            } else if (setpgid(getpid(), pgid) == -1) 
                debug("child set group failed, %d: %s", errno, strerror(errno));
            debug("set process group success, pid %d, ppid %d, cpid %d, pgid %d --CHILD", getpid(), getppid(), getpid(), getpgrp());

            execute_pipeline(curr->first, cpids, cmd_st_idx, &pgid, job);
            debug("Now the value of pgid is %d", pgid);
            cmd_st_idx+=count_cmd_in_pipeline(curr->first);
            curr = curr->rest;
            pl_idx++;
        }
        debug("pid %d pgid %d child exit......................", getpid(), getpgid(getpid()));
        exit(0);
    }
    job->pgid = cpid;
    sf_job_start(job->job_id, cpid);
    
}

void terminate_handler(int sig) {
    // sigset_t new_set, old_set;
    // sigemptyset( &sact.sa_mask );

    for (int i = 0; i < MAX_JOBS; i++)
    {
        JOB* job = &job_table[i];
        if (job->task == NULL) continue;
        JOB_STATUS job_status = job->job_status;
        if (job_status==RUNNING||job_status==CANCELED||job_status==PAUSED) 
        {
            debug("check a job %d==========", getpid());
            check_a_job(job);
            debug("check job %d success......", job->job_id);
        }
    }
    check_and_execute_jobs();
    // sigemptyset(&new_set );
    // sigaddset(&new_set, SIGCHLD);
    // sigprocmask(SIGCHLD, &new_set, &old_set);
}

int check_a_job(JOB* job) {
    int wstatus = -100;
    int result;
    pid_t pgid = job->pgid;
    debug("pid of %d starts to wait for the job%d, pgid %d", getpid(), job->job_id, pgid);
    result = waitpid (pgid, &wstatus, WNOHANG | WUNTRACED | WCONTINUED);
    if (result != pgid) {
        debug("waitpid result of %d not equals to pgid %d, %d--%s", result, pgid, errno, strerror(errno));
    } else {
        debug("waitpid result of %d equals to pgid %d, %d--%s", result, pgid, errno, strerror(errno));
    }
    return process_wstatus(wstatus, job);
}

int process_wstatus(int wstatus, JOB* job) {
    int job_id = job->job_id;
    int pgid = job->pgid;
    if (wstatus == -100) {
        debug("job for pgid %d has not fully completed", job->pgid);
        return wstatus;
    }
    if (WIFEXITED(wstatus)) {
        job->exit_status = wstatus;
        sf_job_end(job_id, pgid, wstatus);
        alter_status(job, COMPLETED);
        debug("job %d was exited, status=%d", job_id, WEXITSTATUS(wstatus));
    } else if (WIFSIGNALED(wstatus)) {
        job->exit_status = wstatus;
        sf_job_end(job_id, pgid, wstatus);
        alter_status(job, ABORTED);
        debug("job %d was killed by signal %d", job_id, WTERMSIG(wstatus));
    } else if (WIFSTOPPED(wstatus)) {
        sf_job_pause(job_id, pgid);
        alter_status(job, PAUSED);
        debug("job %d was stopped by signal %d", job_id, WSTOPSIG(wstatus));
    } else if (WIFCONTINUED(wstatus)) {
        sf_job_resume(job_id, pgid);
        alter_status(job, RUNNING);
        debug("job %d was continued", job_id);
    }
    return wstatus;
}

// TODO: error occurs while(including demo): 
//       1. < and > redirection coexisted
//       2. < and | coexisted
void execute_pipeline(PIPELINE* pl, pid_t cpids[], int cmd_st_idx, pid_t* pgid_ptr, JOB* job) {
    COMMAND_LIST * curr = pl->commands;
    int cmd_idx = 0;
    int curr_fd[2];
    int prev_fd[2];
    
    int in, out;
    if (pl->input_path) {
        in = open(pl->input_path, O_RDONLY);
        verify_fd(in, pl->input_path);
        if (dup2(in, STDIN_FILENO) < 0)  // replace standard input with input file
            debug("dup in failed: %d--%s", errno, strerror(errno));
        close_f(in, pl->input_path);
    }

    while (curr) {
        if (curr->rest) {     // next cmd exists: create pipe
            if (pipe(curr_fd) < 0) {
                debug("Pipe failed"); 
                exit(EXIT_FAILURE);    // TODO
            } else {
                debug("--%d: create pipe success", cmd_idx);
            }
        }
        // create a process
        debug("--%d: Start to fork", cmd_idx);
        pid_t cpid = fork();  // return value is the forked process's child pid
        if (cpid < 0) {
            debug("Fork failed: %s", strerror(errno)); 
            exit(EXIT_FAILURE);    // TODO
        }

        if (cpid == 0) {  // child
            debug("--%d: it's a child", cmd_idx);
            // set group
            if (*pgid_ptr == 0) setpgrp(); 
            else if (setpgid(cpid, *pgid_ptr) == -1) debug("--%d: child set group failed, %d: %s", cmd_idx, errno, strerror(errno));
            debug("--%d: set process group success, pid %d, ppid %d, cpid %d, pgid %d --CHILD", cmd_idx, getpid(), getppid(), cpid, getpgrp());
            
            if (cmd_idx > 0) {     // previous cmd exists: set stdin to pipe
                dup2(prev_fd[0], STDIN_FILENO);
                close_fd(prev_fd);
            }
            if (curr->rest) {   // next cmd exists: set stdout to pipe
                close(curr_fd[0]);
                dup2(curr_fd[1], STDOUT_FILENO);
                close(curr_fd[1]);
            } else if (pl->output_path) {   // next cmd doesn't exist, and output specified
                out = open(pl->output_path, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
                verify_fd(out, pl->output_path);
                dup2(out, STDOUT_FILENO);
            }
            execute_command(curr->first); 
            if (pl->output_path) close_f(out, pl->output_path);
        }

        if (cpid > 0) { // parent
            debug("--%d: it's a parent", cmd_idx);
            cpids[cmd_st_idx+cmd_idx] = cpid;
            debug("the %d cpid is %d", cmd_st_idx+cmd_idx, cpid);

            if (*pgid_ptr == 0) {
                *pgid_ptr = cpid;
                job->pgid = *pgid_ptr;
                sf_job_start(job->job_id, *pgid_ptr);
                alter_status(job, RUNNING);
            }
            // set the process group of children(avoid racing problem)
            if (setpgid(cpid, *pgid_ptr) == -1) debug("--%d: set group failed, %d: %s --PARENT", cmd_idx, errno, strerror(errno));
            debug("--%d: set process group success, pid %d, cpid %d, pgid %d, cpgid: %d --PARENT", cmd_idx, getpid(), cpid, getpgrp(), getpgid(cpid));
            
            if (cmd_idx > 0) close_fd(prev_fd);   // previous cmd exists, close prev_fd
            if (curr->rest) memcpy(prev_fd, curr_fd, sizeof curr_fd);  // next cmd exists: set prev_fd=curr_fd
        }
        cmd_idx++;
        curr = curr->rest;
    }
    
    if (cmd_idx > 1) {  // if there are multiple cmds
        close_fd(prev_fd);
    }

    pid_t p ;
    int status ;
    while ( (p = waitpid(-1,  &status, 0)) > 0)
    {
        debug("parent %d reap child pid %d", getpid(), p) ;
    }
}

void close_fd(int fd[2]) {
    close(fd[0]);
    close(fd[1]);
}

int count_cmd_in_pipeline(PIPELINE* pipeline) {
    COMMAND_LIST* curr = pipeline->commands;
    int cnt = 0;
    while (curr) {
        cnt++;
        curr = curr->rest;
    }
    debug("cmd_cnt in a pipeline is %d", cnt);
    return cnt;
}

int count_cmd_in_task(TASK* task) {
    int cnt = 0;
    PIPELINE_LIST* curr = task->pipelines;
    while (curr)
    {
        cnt+=count_cmd_in_pipeline(curr->first);
        curr = curr->rest;
    }
    return cnt;
}

void verify_fd(int fd, char* path) {
    if (fd == -1) {
        debug("open %s failed: %d-%s", path, errno, strerror(errno));
        exit(EXIT_FAILURE);    // TODO
    } 
    debug("open %s success", path);   
}

void close_f(int fd, char* path) {
    if (fd && close(fd) == -1) {
        debug("close %s failed: %d-%s", path, errno, strerror(errno));
        exit(EXIT_FAILURE);    // TODO
    }
    debug("close %s success", path);
}

void execute_command(COMMAND* command) {
    WORD_LIST * words = command->words;

    // Basically the following codes are constructing an char* argv[], then pass to execvp.
    int cmd_len = 2;
    WORD_LIST * curr_word = words;
    while (curr_word->rest) {
        cmd_len++;
        curr_word = curr_word->rest;
    }

    char * argv[cmd_len];
    argv[cmd_len-1] = NULL;
    curr_word = words;
    int i = 0;
    while (curr_word) {
        argv[i] = curr_word->first;
        curr_word = curr_word->rest;
        i++;
    }
    debug("printing argv...");
    for(int i = 0; i < cmd_len; i++) {
        debug("%d: %s", i, argv[i]);
    }
    if (execvp(words->first, argv) == -1)     // TODO: what if it failed
    {
        debug("error occurs in execvp %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

int print_exit(char* err_msg) {
    printf("%s\n", err_msg);
    return -1;
}

void alter_status(JOB* job, JOB_STATUS job_status) {
    sf_job_status_change(job->job_id, job->job_status, job_status);
    job->job_status = job_status;
}
