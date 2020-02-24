#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "jobber.h"
#include "sf_readline.h"
#include "task.h"
#include "helper_func.h"

/*
 * "Jobber" job spooler.
 */

int main(int argc, char *argv[])
{
    jobs_init();

    // sf_set_readline_signal_hook(signal_hook_func);
    char *in;
    char *second_w;
    while ((in = sf_readline("Jobber> ")) != NULL)
    {
        if (command_eq("help", in))
        {
            printf("Available commands:\n\
help (0 args) Print this help message\n\
quit (0 args) Quit the program\n\
enable (0 args) Allow jobs to start\n\
disable (0 args) Prevent jobs from starting\n\
spool (1 args) Spool a new job\n\
pause (1 args) Pause a running job\n\
resume (1 args) Resume a paused job\n\
cancel (1 args) Cancel an unfinished job\n\
expunge (1 args) Expunge a finished job\n\
status (1 args) Print the status of a job\n\
jobs (0 args) Print the status of all jobs\n");
        }
        else if (command_eq("quit", in))
        {
            jobs_fini();
            exit(0);
        }
        else if (command_eq("enable", in))
        {
            debug("into enable");
            jobs_set_enabled(1);
        }
        else if (command_eq("disable", in))
        {
            debug("into disable");
            jobs_set_enabled(0);
        }
        else if (command_eq("jobs", in))
        {
            debug("into jobs");
            printf("Starting jobs is ");
            if (jobs_get_enabled()==1) printf("enabled"); else printf("disabled");
            printf("\n");
            for (int i = 0; i < MAX_JOBS; i++) {
                char* desc = job_desc(i);
                if (desc != NULL) printf("%s\n", desc);
            }
        }
        else if ((second_w = second_word("spool", in)) != NULL)
        {
            debug("into spool, second_w: %s", second_w);
            char *delimeter = "'";
            char *ptr = strtok(second_w, delimeter);
            int arg_num = 0;
            char *task_command = '\0';
            while (ptr != NULL)
            {
                if (arg_num == 0)
                {
                    task_command = ptr;
                }
                ptr = strtok(NULL, delimeter);
                arg_num++;
            }
            if (arg_num > 1)
            {
                printf("Wrong number of args (given: %d, required: 1) for command 'spool'\n", arg_num);
            }
            else
            {
                if (job_create(task_command) == -1)
                {
                    printf("Error: spool\n");
                }
            }
            task_command = '\0';
            printf("%s", task_command);
        }
        else
        {
            int job_id;
            if ((job_id = get_job_id("pause", in)) != -1)
            {
                debug("into pause");
                job_pause(job_id);
            }
            else if ((job_id = get_job_id("resume", in)) != -1)
            {
                debug("into resume");
                job_resume(job_id);
            }
            else if ((job_id = get_job_id("cancel", in)) != -1)
            {
                debug("into cancel");
                job_cancel(job_id);
            }
            else if ((job_id = get_job_id("expunge", in)) != -1)
            {
                debug("into expunge");
                job_expunge(job_id);
            }
            else if ((job_id = get_job_id("status", in)) != -1)
            {
                debug("into status");
                char* desc = job_desc(job_id);
                if (desc != NULL) printf("%s\n", desc);
            }
            else if (*in)
            {
                printf("Unrecognized command: %s\n", in);
            }
        }
    }
    jobs_fini();
    exit(0);
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
