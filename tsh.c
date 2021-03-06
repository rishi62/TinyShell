    /* 
     * tsh - A tiny shell program with job control
     * 
     * Name:Rishikesh Bhatt
     * Email-id:201501062@daiict.ac.in
     */
    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <string.h>
    #include <ctype.h>
    #include <signal.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <errno.h>

    /* Misc manifest constants */
    #define MAXLINE    1024   /* max line size */
    #define MAXARGS     128   /* max args on a command line */
    #define MAXJOBS      16   /* max jobs at any point in time */
    #define MAXJID    1<<16   /* max job ID */

    /* Job states */
    #define UNDEF 0 /* undefined */
    #define FG 1    /* running in foreground */
    #define BG 2    /* running in background */
    #define ST 3    /* stopped */

    /* 
     * Jobs states: FG (foreground), BG (background), ST (stopped)
     * Job state transitions and enabling actions:
     *     FG -> ST  : ctrl-z
     *     ST -> FG  : fg command
     *     ST -> BG  : bg command
     *     BG -> FG  : fg command
     * At most 1 job can be in the FG state.
     */

    /* Global variables */
    extern char **environ;      /* defined in libc */
    char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
    int verbose = 0;            /* if true, print additional output */
    int nextjid = 1;            /* next job ID to allocate */
    char sbuf[MAXLINE];         /* for composing sprintf messages */

    struct job_t {              /* The job struct */
            pid_t pid;              /* job PID */
            int jid;                /* job ID [1, 2, ...] */
            int state;              /* UNDEF, BG, FG, or ST */
            char cmdline[MAXLINE];  /* command line */
    };
    struct job_t jobs[MAXJOBS]; /* The job list */
    /* End global variables */


    /* Function prototypes */

    /* Here are the functions that you will implement */
    void eval(char *cmdline);
    int builtin_cmd(char **argv);
    void do_bgfg(char **argv);
    void waitfg(pid_t pid);

    void sigchld_handler(int sig);
    void sigtstp_handler(int sig);
    void sigint_handler(int sig);

    /* Here are helper routines that we've provided for you */
    int parseline(const char *cmdline, char **argv); 
    void sigquit_handler(int sig);

    void clearjob(struct job_t *job);
    void initjobs(struct job_t *jobs);
    int maxjid(struct job_t *jobs); 
    int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
    int deletejob(struct job_t *jobs, pid_t pid); 
    pid_t fgpid(struct job_t *jobs);
    struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
    struct job_t *getjobjid(struct job_t *jobs, int jid); 
    int pid2jid(pid_t pid); 
    void listjobs(struct job_t *jobs);

    void usage(void);
    void unix_error(char *msg);
    void app_error(char *msg);
    typedef void handler_t(int);
    handler_t *Signal(int signum, handler_t *handler);
        /*wrappers*/
    void Kill(pid_t pid,int signal);
    /*
     * main - The shell's main routine 
     */
    int main(int argc, char **argv) 
    {
            char c;
            char cmdline[MAXLINE];
            int emit_prompt = 1; /* emit prompt (default) */

            /* Redirect stderr to stdout (so that driver will get all output
             * on the pipe connected to stdout) */
            dup2(1, 2);

            /* Parse the command line */
            while ((c = getopt(argc, argv, "hvp")) != EOF) {
                    switch (c) {
                    case 'h':             /* print help message */
                            usage();
                break;
                    case 'v':             /* emit additional diagnostic info */
                            verbose = 1;
                break;
                    case 'p':             /* don't print a prompt */
                            emit_prompt = 0;  /* handy for automatic testing */
                break;
        default:
                            usage();
        }
            }

            /* Install the signal handlers */

            /* These are the ones you will need to implement */
            Signal(SIGINT,  sigint_handler);   /* ctrl-c */
            Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
            Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

            /* This one provides a clean way to kill the shell */
            Signal(SIGQUIT, sigquit_handler); 

            /* Initialize the job list */
            initjobs(jobs);

            /* Execute the shell's read/eval loop */
            while (1) {

        /* Read command line */
        if (emit_prompt) {
                printf("%s", prompt);
                fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
                app_error("fgets error");
        if (feof(stdin)) { /* End of file (ctrl-d) */
                fflush(stdout);
                exit(0);
        }

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
            } 

            exit(0); /* control never reaches here */
    }
        
    /* 
     * eval - Evaluate the command line that the user has just typed in
     * 
     * If the user has requested a built-in command (quit, jobs, bg or fg)
     * then execute it immediately. Otherwise, fork a child process and
     * run the job in the context of the child. If the job is running in
     * the foreground, wait for it to terminate and then return.  Note:
     * each child process must have a unique process group ID so that our
     * background children don't receive SIGINT (SIGTSTP) from the kernel
     * when we type ctrl-c (ctrl-z) at the keyboard.  
    */
void eval(char *cmdline){
         sigset_t set1;  
         sigemptyset(&set1);  
         sigaddset(&set1,SIGCHLD);
            int bg;
            int stat;
            char *argv[MAXARGS];
            bg = parseline(cmdline,argv);

            pid_t cpid;
            
        if(!builtin_cmd(argv)){
                sigprocmask(SIG_BLOCK,&set1,NULL);
                if(bg)          stat=BG;
                else            stat = FG;

                
                cpid = fork();
                switch(cpid){
                        /*In case fork fail*/
                        case -1:
                               return;
                        /*Child creates its own process group 
                         * if argument is valid
                         */
                        case 0:{
                                setpgid(0,0);
                                if(execvp(argv[0],argv) == -1){
                                        printf("%s: Command not found\n",argv[0]);
                                        exit(0);
                                }
                                           
                        }
                        /*
                         * Parent will add the job and block the incoming signals and
                         * Check if the process should run in background or foreground
                         */
                        default:{
                                
                                addjob(jobs,cpid,stat,cmdline);
                                sigprocmask(SIG_UNBLOCK,&set1,NULL);
                                
                                if(bg){
                                        struct job_t *job = getjobpid(jobs,cpid);
                                        printf("[%d] (%d)   %s",job->jid,job->pid,job->cmdline);
                                        return;
                                }
                                else    waitfg(cpid);   /*(custom) wait for child*/
                        }
               
                }
        
        }
            return;
}

    /* 
     * parseline - Parse the command line and build the argv array.
     * 
     * Characters enclosed in single quotes are treated as a single
     * argument.  Return true if the user has requested a BG job, false if
     * the user has requested a FG job.  
     */
    int parseline(const char *cmdline, char **argv) 
    {
            static char array[MAXLINE]; /* holds local copy of command line */
            char *buf = array;          /* ptr that traverses command line */
            char *delim;                /* points to first space delimiter */
            int argc;                   /* number of args */
            int bg;                     /* background job? */

            strcpy(buf, cmdline);
            buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
            while (*buf && (*buf == ' ')) /* ignore leading spaces */
        buf++;

            /* Build the argv list */
            argc = 0;
            if (*buf == '\'') {
        buf++;
        delim = strchr(buf, '\'');
            }
            else {
        delim = strchr(buf, ' ');
            }

            while (delim) {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* ignore spaces */
                     buf++;

        if (*buf == '\'') {
                buf++;
                delim = strchr(buf, '\'');
        }
        else {
                delim = strchr(buf, ' ');
        }
            }
            argv[argc] = NULL;
            
            if (argc == 0)  /* ignore blank line */
        return 1;

            /* should the job run in the background? */
            if ((bg = (*argv[argc-1] == '&')) != 0) {
        argv[--argc] = NULL;
            }
            return bg;
    }

    /* 
     * builtin_cmd - If the user has typed a built-in command then execute
     *    it immediately.  
     */
    int builtin_cmd(char **argv) 
    {
                /*
                 * Checking if process are stopped in the background 
                 * Then the shell should prompt a message to stop the jobs.
                 * We will get the shell prompt back       
                 */
            if(strcmp(argv[0],"quit") == 0)
            {
                int i;
                for(i = 0;i<MAXJOBS;i++)
                {
                    if(jobs[i].state == ST)     
                        {
                            printf("There are jobs which are stopped!! Terminate them\nUse kill -9 <pid>\n");
                            listjobs(jobs);
                            return 1;
                        }
                }
                    exit(0);
            }
            /*List the current jobs*/
            if(strcmp(argv[0],"jobs") == 0)
            {
                    listjobs(jobs);
                    return 1;
            }

            /*For job control: foregrounding or backgrounding the jobs using do_bgfg*/
            if(strcmp(argv[0],"fg") == 0 || strcmp(argv[0],"bg") == 0)
            {
                    do_bgfg(argv);
                    return 1;
            }
            return 0;     /* not a builtin command */
    }


        /*
         * check if the current string is number or not
         */
     int numbers_only(const char *s)
    {
            while (*s) {
                    if (isdigit(*s++) == 0) return 0;
            }

            return 1;
    }


    /*
     * Check if the current string structure is of the job id i.e %<integer>
     */
    int is_job_id(const char *s){
        if(*s != '%')
                return 0;
        s++;
        return numbers_only(s);
    }
    /* 
     * do_bgfg - Execute the builtin bg and fg commands
     */

    void do_bgfg(char **argv) 
    {
        /*Error handling for invalid commands*/
        if(argv[1] == NULL){
        printf("%s command requires PID or %%jobid argument\n",argv[0]);
        return;
        }

        /*If process id it should go in*/
        if(numbers_only(argv[1])){
                struct job_t *job;
                int pid = atoi(argv[1]);;
                job = getjobpid(jobs,pid);
                /*If job dont exists*/
                if(job == NULL) {
                        printf("(%s): No such process\n",argv[1]);
                        return ;
                }  
                
                switch(argv[0][0]){

                        /*if command is fg*/
                        case 'f':{
                                  
                                        switch(job->state){
                                                /*
                                                 * If the State is stopped then shell will pass
                                                 *  a sigcont signal to start the program in background
                                                 */        
                                                case ST:{
                                                job->state = FG;
                                                Kill(-(job->pid),SIGCONT);
                                                waitfg(job->pid);
                                                break;
                                                }
                                                /*
                                                 * If the state is background and running then the
                                                 * change the state to foreground and wait for it (by calling waitfg)
                                                 */
                                                case BG:{
                                                job->state = FG;
                                                waitfg(job->pid);
                                                break;
                                                }

                                                default:{}    
                                        }
                                        break;
                        }
                        /*Command is bg*/
                        case 'b':{

                                /*
                                 * If state is stopped then start the proggram and push it to  
                                 * background
                                 */
                                switch(job->state){
                                        case ST:{
                                        job->state = BG;
                                        Kill(-(job->pid),SIGCONT);
                                        printf("[%d] (%d)  %s",job->jid,job->pid,job->cmdline );
                                        break;
                                        }

                                        default:{
                                        printf("process [%d] Already in background\n",job->pid);
                                        }
                                }
                                break;
                        }       
                        default:
                        return;
                }
                    
                return;
        }
        /* The argument is a job id*/
        else if(is_job_id(argv[1])){
                
                int len = strlen(argv[1]);
                  for(int i = 0;i< len-1;i++)
                         argv[1][i] = argv[1][i+1];
                argv[1][len-1] = '\0';
                
                struct job_t *job;
                job = getjobjid(jobs,atoi(argv[1]));

                if(job == NULL) {
                printf("%%%s: No such job\n",argv[1]);
                return ;
                }   
                
                switch(argv[0][0]){
                        /*if command is fg*/
                        case 'f':{
                                
                                switch(job->state){

                                        /*
                                         * If the State is stopped then shell will pass
                                         *  a sigcont signal to start the program in background
                                         */
                                        case ST:{
                                        job->state = FG;
                                        Kill(-(job->pid),SIGCONT);
                                        waitfg(job->pid);
                                        break;
                                        }
                                        /*
                                         * If the state is background and running then the
                                         * change the state to foreground and wait for it (by calling waitfg)
                                         */
                                        case BG:{
                                        job->state = FG;
                                        waitfg(job->pid);
                                        break;
                                        }

                                        default:
                                        {
                                              unix_error("Switch Internal error");
                                        }
                                } 

                                break;
                        }
                        /*Command is bg*/
                        case 'b':{

                                switch(job->state){
                                /*
                                 * If state is stopped then start the proggram and push it to  
                                 * background
                                 */
                                    case ST:
                                    {
                                     job->state = BG;
                                     Kill(-(job->pid),SIGCONT);
                                     printf("[%d] (%d)  %s",job->jid,job->pid,job->cmdline );
                                     break;
                                    }
                                    default:
                                    {printf("Job [%d] already in background\n",job->jid );}
                                }
                                
                                break;
                        }

                        default:{
                                unix_error("Switch Internal error");
                        }
                        
                }
                return;
        }
        else{
        printf("%s: argument must be a PID or %%jobid\n",argv[0]);
        }
        
        return;
    }

    /* 
     * waitfg - Block until process pid is no longer the foreground process
     */
    void waitfg(pid_t pid)
    {
        struct job_t *job;
        job = getjobpid(jobs,pid);
        /*If the process is in foreground the wait bys calling sleep()*/
        while(job->state == FG)
        {
            sleep(1);
        }
        /* when argument -v is passed*/
        if(verbose)
                printf("waitfg: (%d) Process no longer the fg process\n",pid);
         return;
    }

    /*****************
     * Signal handlers
     *****************/

    /* 
     * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
     *     a child job terminates (becomes a zombie), or stops because it
     *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
     *     available zombie children, but doesn't wait for any other
     *     currently running children to terminate.  
     */
void sigchld_handler(int sig){
    int stat;
    pid_t pid;
    struct job_t *job;
   
         if(verbose){
                printf("sigchld_handler: entering\n");
        }   
        
        /* loop until all the child are reaped
         * It is invoked when child exits,terminates by a signal or
         * stopped by a signal
         * For options WNOHANG and WUNTRACED refer to wait manpages
         */
        while((pid = waitpid(-1,&stat,WNOHANG | WUNTRACED)) > 0){
            job = getjobpid(jobs,pid);

            /*If exited normally delete the job*/
                if(WIFEXITED(stat)){
                        if(verbose){
                                printf("sigchld_handler: Job [%d] (%d) deleted\n",job->jid,job->pid);
                                printf("sigchld_handler: Job [%d] (%d) terminates Ok (status %d)\n",job->jid,job->pid,stat );
                        }
                deletejob(jobs,pid);
            }
            /*If terminated due to a signal specify the signal and delete the job*/
                else if(WIFSIGNALED(stat)){ 
                        if(verbose)
                                printf("sigchld_handler: Job [%d] (%d) deleted\n",job->jid,job->pid);
                        
                        printf("Job [%d] (%d) terminated by signal %d\n",job->jid,job->pid,WTERMSIG(stat));
                        deletejob(jobs,pid);
                }
                /* If stopped by the signal specify the signal change the state to ST and dont delete the job*/
                else if(WIFSTOPPED(stat)){
                        job->state = ST;
                        printf("Job [%d] (%d) stopped by signal %d\n", job->jid,job->pid,WSTOPSIG(stat));
                }
        }
        
                if(verbose)
                        printf("sigchld_handler: exiting\n");
            return;
}

    /* 
     * sigint_handler - The kernel sends a SIGINT to the shell whenver the
     *    user types ctrl-c at the keyboard.  Catch it and send it along
     *    to the foreground job.  
     */
    void sigint_handler(int sig) 
    {
        pid_t fpid;
        if(verbose)
                printf("sigint_handler: entering\n");
        
        if((fpid = fgpid(jobs)) > 0)
        {
                /*Wrapper for kill function;killing all the process of the given process's group */
            Kill(-fpid,SIGINT);    
            if(verbose)
                 printf("sigint_handler: Job (%d) killed\n",fpid);
        }
        if(verbose)
                printf("sigint_handler: exiting\n");
               
        
            return;
    } 

    /*
     * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
     *     the user types ctrl-z at the keyboard. Catch it and suspend the
     *     foreground job by sending it a SIGTSTP.  
     */
    void sigtstp_handler(int sig) 
    {
        if(verbose)
                printf("sigstp_handler: entering\n");
        pid_t fpid;
        fpid = fgpid(jobs);
        struct job_t *job = getjobpid(jobs,fpid);
        if(fpid > 0)
        {
                /*
                 * Wrapper for kill function
                 * Stopping all the processes of the current process's group
                 */
                 Kill(-fpid,SIGTSTP);
                if(verbose)
                 printf("sigstp_handler: Job [%d] (%d)stopped\n",job->jid,fpid);
        }
        if(verbose)
                printf("sigstp_handler: exiting\n");
            return;
    }

    /*********************
     * End signal handlers
     *********************/

    /***********************************************
     * Helper routines that manipulate the job list
     **********************************************/
    /*SetpGroup: Child should create a new process group with itself as the group leader*/
     /*void setpgrp(pid_t pid){

     } */


    /* clearjob - Clear the entries in a job struct */
    void clearjob(struct job_t *job) {
            job->pid = 0;
            job->jid = 0;
            job->state = UNDEF;
            job->cmdline[0] = '\0';
    }

    /* initjobs - Initialize the job list */
    void initjobs(struct job_t *jobs) {
            int i;

            for (i = 0; i < MAXJOBS; i++)
        clearjob(&jobs[i]);
    }

    /* maxjid - Returns largest allocated job ID */
    int maxjid(struct job_t *jobs) 
    {
            int i, max=0;

            for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid > max)
                max = jobs[i].jid;
            return max;
    }

    /* addjob - Add a job to the job list */
    int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
    {
            int i;
            
            if (pid < 1)
        return 0;

            for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == 0){
                jobs[i].pid = pid;
                jobs[i].state = state;
                jobs[i].jid = nextjid++;
                if (nextjid > MAXJOBS)
            nextjid = 1;
                strcpy(jobs[i].cmdline, cmdline);
                    if(verbose){
                        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
                            }
                            return 1;
        }
            }
            printf("Tried to create too many jobs\n");
            return 0;
    }

    /* deletejob - Delete a job whose PID=pid from the job list */
    int deletejob(struct job_t *jobs, pid_t pid) 
    {
            int i;

            if (pid < 1)
        return 0;

            for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
                clearjob(&jobs[i]);
                nextjid = maxjid(jobs)+1;
                return 1;
        }
            }
            return 0;
    }

    /* fgpid - Return PID of current foreground job, 0 if no such job */
    pid_t fgpid(struct job_t *jobs) {
            int i;

            for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].state == FG)
                return jobs[i].pid;
            return 0;
    }

    /* getjobpid  - Find a job (by PID) on the job list */
    struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
            int i;

            if (pid < 1)
        return NULL;
                for (i = 0; i < MAXJOBS; i++)
                        if (jobs[i].pid == pid)
                                return &jobs[i];
        return NULL;
    }

    /* getjobjid  - Find a job (by JID) on the job list */
    struct job_t *getjobjid(struct job_t *jobs, int jid) 
    {
            int i;

        if (jid < 1)
            return NULL;
        for (i = 0; i < MAXJOBS; i++)
                if (jobs[i].jid == jid)
                        return &jobs[i];
        return NULL;
    }

    /* pid2jid - Map process ID to job ID */
    int pid2jid(pid_t pid){
        int i;

        if (pid < 1)
            return 0;
        for (i = 0; i < MAXJOBS; i++)
                if (jobs[i].pid == pid) {
                        return jobs[i].jid;
                }
        return 0;
    }

    /* listjobs - Print the job list */
void listjobs(struct job_t *jobs){
            int i;
            
        for (i = 0; i < MAXJOBS; i++) {
                if (jobs[i].pid != 0) {
                        printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
                        
                        switch (jobs[i].state) {
                                case BG: 
                                        printf("Running ");
                                        break;
                                case FG: 
                                        printf("Foreground ");
                                        break;
                                case ST: 
                                        printf("Stopped ");
                                        break;
                                default:
                                        printf("listjobs: Internal error: job[%d].state=%d ", 
                                        i, jobs[i].state);
                        }
                                printf("%s", jobs[i].cmdline);
                }
            
        }
    
}
    /******************************
     * end job list helper routines
     ******************************/


    /***********************
     * Other helper routines
     ***********************/

    /*
     * usage - print a help message
     */
    void usage(void){
            printf("Usage: shell [-hvp]\n");
            printf("   -h   print this message\n");
            printf("   -v   print additional diagnostic information\n");
            printf("   -p   do not emit a command prompt\n");
            exit(1);
    }

    /*
     * unix_error - unix-style error routine
     */
    void unix_error(char *msg){
            fprintf(stderr, "%s: %s\n", msg, strerror(errno));
            exit(1);
    }

    /*
     * app_error - application-style error routine
     */
    void app_error(char *msg){
            fprintf(stdout, "%s\n", msg);
            exit(1);
    }

    /*
     * Signal - wrapper for the sigaction function
     */
    handler_t *Signal(int signum, handler_t *handler){
            struct sigaction action, old_action;

            action.sa_handler = handler;  
            sigemptyset(&action.sa_mask); /* block sigs of type being handled */
            action.sa_flags = SA_RESTART; /* restart syscalls if possible */

            if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
            return (old_action.sa_handler);
    }

    /*
     *  kill - Wrapper for kill function
     *  For Error handling
     */

     void Kill(pid_t pid,int sig){
        if(kill(pid,sig) < 0)
                unix_error("kill failed ");
     }
    /*
     * sigquit_handler - The driver program can gracefully terminate the
     *    child shell by sending it a SIGQUIT signal.
     */
    void sigquit_handler(int sig){
            printf("Terminating after receipt of SIGQUIT signal\n");
            exit(1);
    }



