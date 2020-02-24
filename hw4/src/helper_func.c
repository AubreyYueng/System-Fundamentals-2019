#include "../include/helper_func.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "debug.h"
#include "jobber.h"

int command_eq(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre), lenstr = strlen(str);
    if (lenstr >= lenpre && memcmp(pre, str, lenpre) == 0)
    {
        for (size_t i = 0; i < lenpre; i++)
        {
            str++;
        }
        while (*str)
        {
            if (!isspace(*str))
            {
                printf("Wrong number of args (required: 0) for command '%s'\n", pre);
                return 0;
            }
            str++;
        }
        return 1;
    }
    return 0;
}

/**
 * returns start pointer after pre(skip space), otherwise returns NULL
 */
char* second_word(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre), lenstr = strlen(str);
    if (lenstr >= lenpre && memcmp(pre, str, lenpre) == 0)
    {
        for (size_t i = 0; i < lenpre; i++)
        {
            str++;
        }
        while (*str && isspace(*str))
        {
            str++;
        }

        if (!*str)
        {
            printf("Wrong number of args (given: 0, required: 1) for command '%s'\n", pre);
            return NULL;
        }
        return (char* )str;
    }
    return NULL;
}

int get_job_id(const char *pre, const char *str) 
{
    char* second_w = second_word(pre, str);
    if (second_w == NULL)
    {
        return -1;
    }
    
    int job_id;
    sscanf(second_w, "%d", &job_id);
    debug("job_id is %d", job_id);
    return job_id;
}

char* job_desc(int jobid) {
    JOB_STATUS job_status = job_get_status(jobid);
    if (job_status == -1) return NULL;
    char desc[1024];
    char *desc_ptr = desc;
    char jobid_str[10];
    sprintf(jobid_str, "%d", jobid);

    strcpy(desc, "job ");
    strcat(desc, jobid_str);
    strcat(desc, " [");
    strcat(desc, job_status_names[job_status]);
    strcat(desc, "]: ");

    char* task_str = job_get_taskspec(jobid);
    if (task_str == NULL) return NULL;
    strcat(desc, task_str);

    return desc_ptr;    
}
