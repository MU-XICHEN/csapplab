/*
 * tsh - A tiny shell program with job control
 *
 * <Put your name and login ID here>
 */
#define _POSIX_C_SOURCE 199309L
#define _GNU_SOURCE

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
#define MAXLINE 1024   /* max line size */
#define MAXARGS 128    /* max args on a command line */
#define MAXJOBS 16     /* max jobs at any point in time */
#define MAXJID 1 << 16 /* max job ID */

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
extern char **environ;   /* defined in libc */
char prompt[] = "tsh> "; /* command line prompt (DO NOT CHANGE) */
int verbose = 0;         /* if true, print additional output */
int nextjid = 1;         /* next job ID to allocate */
char sbuf[MAXLINE];      /* for composing sprintf messages */

struct job_t
{                          /* The job struct */
    pid_t pid;             /* job PID */
    int jid;               /* job ID [1, 2, ...] */
    int state;             /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE]; /* command line */
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

int is_signal_blocked(int);

ssize_t sio_puts(char s[]);
ssize_t sio_putl(long v);
void sio_error(char s[]);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine
 */
int main(int argc, char **argv)
{
    // printf("[TSH PID]: %d \n", getpid());
    fflush(stdout);
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF)
    {
        switch (c)
        {
        case 'h': /* print help message */
            usage();
            break;
        case 'v': /* emit additional diagnostic info */
            verbose = 1;
            break;
        case 'p':            /* don't print a prompt */
            emit_prompt = 0; /* handy for automatic testing */
            break;
        default:
            usage();
        }
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT, sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler); /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler); /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler);

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1)
    {

        /* Read command line */
        if (emit_prompt)
        {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin))
        { /* End of file (ctrl-d) */
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
void eval(char *cmdline)
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if (argv[0] == NULL)
        return; /* Ignore empty lines */

    sigset_t mask_all, mask_one, prev;

    sigemptyset(&mask_all);
    sigaddset(&mask_one, SIGCHLD);

    sigfillset(&mask_all);

    if (!builtin_cmd(argv))
    {
        sigprocmask(SIG_BLOCK, &mask_one, &prev); // 确保 SIG_CHILD 信号不会在 addjob 之前到达

        if ((pid = fork()) == 0)
        {
            setpgid(0, 0);
            sigprocmask(SIG_SETMASK, &prev, NULL); // 给子进程解开 SIG_CHILD，并继承其他的信号状态，避免自身无法处理后续子进程的信号

            // ❗❗❗注意：execve 会覆盖handler，不会覆盖 mask
            if (execve(argv[0], argv, environ) < 0)
            {
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }

        int state = bg ? BG : FG;

        sigprocmask(SIG_BLOCK, &mask_all, NULL); /* Parent process */
        addjob(jobs, pid, state, cmdline);       /* Add the child to the job list */

        pid_t fg_pid = fgpid(jobs);

        /**
         *  foreground job
         */
        // 当 fg_pid 存在，说明存在前台进程
        while (fg_pid)
        {
            // 这里同时需要 unblock 其他信号，比如 SIGINT，tsh 要对这些信号做转发，转发后子进程才可以退出
            sigsuspend(&prev); // 临时 unblock
            // 休眠途中收到信号并处理后，会返回当前进行执行，前面 sigsuspend 能保证信号处理完之后 再继续block原来的位向量
            fg_pid = fgpid(jobs); // 如果前面一次SIGCHLD唤醒了休眠，并 delete了，此时拿到的 fg_pid 就是0
        }

        /**
         * background job
         */
        if (state == BG)
        {
            struct job_t *running_job = getjobpid(jobs, pid);
            printf("[%d] (%d) %s", running_job->jid, running_job->pid, cmdline);
        }

        // 给后台进程恢复信号
        sigprocmask(SIG_SETMASK, &prev, NULL); /* Unblock SIGCHLD */
    }
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
    buf[strlen(buf) - 1] = ' ';   /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'')
    {
        buf++;
        delim = strchr(buf, '\'');
    }
    else
    {
        delim = strchr(buf, ' ');
    }

    while (delim)
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* ignore spaces */
            buf++;

        if (*buf == '\'')
        {
            buf++;
            delim = strchr(buf, '\'');
        }
        else
        {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;

    if (argc == 0) /* ignore blank line */
        return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
    {
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
    sigset_t mask_all, prev;
    sigfillset(&mask_all);

    if (!strcmp(argv[0], "quit")) /* quit command */
        exit(0);

    if (!strcmp(argv[0], "jobs"))
    {
        sigprocmask(SIG_BLOCK, &mask_all, &prev); // sigprocmask 之前允许被其他处理程序中断
        listjobs(jobs);
        sigprocmask(SIG_SETMASK, &prev, NULL);

        return 1;
    }

    if (!strcmp(argv[0], "bg") || !strcmp(argv[0], "fg"))
    {
        do_bgfg(argv);
        return 1;
    }

    return 0; /* not a builtin command */
}

/*
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv)
{
    pid_t pid = 0;
    int jid = 0;
    int isBG = !strcmp(argv[0], "bg");

    if (!(argv[1]))
    {
        printf("%s command requires PID or %%jobid argument \n", isBG ? "bg" : "fg");
        fflush(stdout);
        return;
    }

    char *endptr;

    char *id_str = argv[1];

    if (id_str[0] == '%')
    {
        jid = strtol(id_str + 1, &endptr, 10);
    }
    else
    {
        pid = strtol(id_str, &endptr, 10);
    }

    if (endptr == argv[1])
    {
        printf("%s: argument must be a PID or %%jobid \n", isBG ? "bg" : "fg");
        fflush(stdout);
        return;
    }

    sigset_t mask_all, prev_all;
    sigfillset(&mask_all);

    // 访问 jobs 期间阻塞其他信号
    sigprocmask(SIG_BLOCK, &mask_all, &prev_all);

    struct job_t *job = pid ? getjobpid(jobs, pid) : getjobjid(jobs, jid);

    if (!job)
    {
        // 不存在
        if (pid)
        {
            printf("(%d): No such process \n", pid);
        }
        if (jid)
        {
            printf("%%%d: No such job \n", jid);
        }
        fflush(stdout);
    }
    else
    {
        // 存在
        if (kill(-job->pid, SIGCONT) < 0)
        {
            // fail
            printf("SEND SIGCONT ERROR");
        }
        else
        {
            // succ，更新 jobs
            if (isBG)
            {
                job->state = BG;
                printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
                fflush(stdout);
            }
            else
            {
                job->state = FG;
            }
        }
    }

    sigprocmask(SIG_SETMASK, &prev_all, NULL); // 恢复
}

/*
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    return;
}

/*****************
 * Signal handlers
 *****************/

/*
 * reference-link: https://web.stanford.edu/class/archive/cs/cs110/cs110.1204/static/lectures/07-Signals/lecture-07-signals.pdf#:~:text=To%20reiterate%3A%20If%20one%20or,in%20a%20loop%2C%20as%20above
 *
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.
 *
 */
void sigchld_handler(int sig)
{
    int olderrno = errno;

    sigset_t mask_all, prev_all;
    pid_t pid;
    sigfillset(&mask_all);

    /**
     * 情景说明:
     *      (1) 在 waitpid 执行前的子进程都可以被此次 while 正常回收
     *      (2) 如果在处理程序过程中结束的子进程，会因为 pending 状态，下一次内核触发处理程序时回收
     */

    int status;

    // 处理已经终止的进程
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) // WNOHANG | WUNTRACED - 没有停止或终止的子进程的时候立马返回0，否则返回对应的pid
    {
        /* Reap a zombie child */
        sigprocmask(SIG_BLOCK, &mask_all, &prev_all); // sigprocmask 之前允许被其他处理程序中断
        struct job_t *job = getjobpid(jobs, pid);

        if (WIFSIGNALED(status) || WIFEXITED(status))
        {

            if (WIFSIGNALED(status))
            {
                // 终止: 信号退出
                printf("Job [%d] (%d) terminated by signal %d \n", job->jid, job->pid, SIGINT);
            }
            deletejob(jobs, pid);
        }
        else if (WIFSTOPPED(status))
        {
            // 停止
            job->state = ST;
            printf("Job [%d] (%d) stopped by signal %d \n", job->jid, job->pid, SIGTSTP);
        }

        fflush(stdout);
        sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }

    errno = olderrno;
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 */
void sigint_handler(int sig)
{
    /**
     * 对于 tsh 来说，子进程不会直接收到 SIGINT 信号，外部通过 kill pid 指定pid的形式发送
     * 同时，因为 execve 会覆盖子进程的处理程序，所以转发的内容也不用担心子进程会使用当前这个处理程序
     * 因此，这里只是单纯的转发
     */
    pid_t fg_pid;

    sigset_t mask_all, prev_all;
    sigfillset(&mask_all);

    // 访问期间避免其他信号修改 jobs，
    sigprocmask(SIG_BLOCK, &mask_all, &prev_all); // sigprocmask 之前允许被其他处理程序中断
    fg_pid = fgpid(jobs);

    if (fg_pid)
    {
        if (kill(-fg_pid, sig) < 0)
        {
            sio_puts("[sigint_handler] transfer error \n");
        }
    }

    sigprocmask(SIG_SETMASK, &prev_all, NULL);
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
void sigtstp_handler(int sig)
{
    pid_t fg_pid;

    sigset_t mask_all, prev_all;
    sigfillset(&mask_all);

    // 访问期间避免其他信号修改 jobs，
    sigprocmask(SIG_BLOCK, &mask_all, &prev_all); // sigprocmask 之前允许被其他处理程序中断
    fg_pid = fgpid(jobs);

    if (fg_pid)
    {
        if (kill(-fg_pid, sig) < 0)
        {
            sio_puts("[sigtstp_handler] transfer error \n");
        }
    }

    sigprocmask(SIG_SETMASK, &prev_all, NULL);
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job)
{
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
        clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs)
{
    int i, max = 0;

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

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid == 0)
        {
            jobs[i].pid = pid;
            jobs[i].state = state;
            jobs[i].jid = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            strcpy(jobs[i].cmdline, cmdline);
            if (verbose)
            {
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

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid == pid)
        {
            clearjob(&jobs[i]);
            nextjid = maxjid(jobs) + 1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].state == FG)
            return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid)
{
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
int pid2jid(pid_t pid)
{
    int i;

    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
        {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid != 0)
        {
            printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
            switch (jobs[i].state)
            {
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

int is_signal_blocked(int sig)
{
    sigset_t current_mask;

    // 获取当前信号掩码
    if (sigprocmask(SIG_SETMASK, NULL, &current_mask) == -1)
    {
        return -1; // 错误
    }

    // 检查信号是否在集合中
    return sigismember(&current_mask, sig);
}

/* Private sio functions */

/* $begin sioprivate */
/* sio_reverse - Reverse a string (from K&R) */
static void sio_reverse(char s[])
{
    int c, i, j;

    for (i = 0, j = strlen(s) - 1; i < j; i++, j--)
    {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* sio_ltoa - Convert long to base b string (from K&R) */
static void sio_ltoa(long v, char s[], int b)
{
    int c, i = 0;
    int neg = v < 0;

    if (neg)
        v = -v;

    do
    {
        s[i++] = ((c = (v % b)) < 10) ? c + '0' : c - 10 + 'a';
    } while ((v /= b) > 0);

    if (neg)
        s[i++] = '-';

    s[i] = '\0';
    sio_reverse(s);
}

/* sio_strlen - Return length of string (from K&R) */
static size_t sio_strlen(char s[])
{
    int i = 0;

    while (s[i] != '\0')
        ++i;
    return i;
}
/* $end sioprivate */

ssize_t sio_puts(char s[]) /* Put string */
{
    return write(STDOUT_FILENO, s, sio_strlen(s)); // line:csapp:siostrlen
}

ssize_t sio_putl(long v) /* Put long */
{
    char s[128];

    sio_ltoa(v, s, 10); /* Based on K&R itoa() */ // line:csapp:sioltoa
    return sio_puts(s);
}

void sio_error(char s[]) /* Put error message and exit */
{
    sio_puts(s);
    _exit(1); // line:csapp:sioexit
}

/*
 * usage - print a help message
 */
void usage(void)
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig)
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}
