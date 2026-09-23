#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <pwd.h>

#define DEBUG_DAEMON_LOG "/mnt/nand/debug_daemon.flag"

#define ANJ_APP "/opt/ch/anjcam"

#define MAX_FORK_COUNT 5
#define MIN_FORK_DIF_TIME 10000.0 // 10 seconds

int run_flag = 0;
#define NO_SYSCALL 0x1 << 1

typedef struct
{
    int fork_times;
    double fork_time[MAX_FORK_COUNT];
} PROCESS_ENTYR;

enum
{
    PROCESS_APP = 0,
    PROCESS_INDEX_MAX,
};

int bExit = 0;
int bEnd = 0;
PROCESS_ENTYR g_process_monitor_list[PROCESS_INDEX_MAX];

void process_monitor_init(void)
{
    for (int i = 0; i < PROCESS_INDEX_MAX; i++)
    {
        g_process_monitor_list[i].fork_times = 0;
        for (int j = 0; j < MAX_FORK_COUNT; j++)
        {
            g_process_monitor_list[i].fork_time[j] = 0.0;
        }
    }
}

int process_fork_and_check(int process_index)
{
    struct timeval tv;
    double fork_time;
    double fork_dif;
    double min_fork_dif;

    int fork_index;
    int fork_too_fast;
    int i;

    gettimeofday(&tv, NULL);
    fork_time = tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;

    fork_index = g_process_monitor_list[process_index].fork_times;

    // check if time change
    if (fork_index > 0 && g_process_monitor_list[process_index].fork_time[fork_index - 1] > fork_time)
    {
        fork_index = 0;
        g_process_monitor_list[process_index].fork_times = 0;
    }

    if (fork_index == MAX_FORK_COUNT)
    {
        printf("process: %d, fork_time: \n",
               process_index);

        for (i = 0; i < MAX_FORK_COUNT - 1; i++)
        {
            g_process_monitor_list[process_index].fork_time[i] = g_process_monitor_list[process_index].fork_time[i + 1];
            printf("%f\n", g_process_monitor_list[process_index].fork_time[i]);
        }

        g_process_monitor_list[process_index].fork_time[fork_index - 1] = fork_time;

        printf("%f\n", fork_time);

        fork_too_fast = 1;
        for (i = 1; i < MAX_FORK_COUNT; i++)
        {
            fork_dif = g_process_monitor_list[process_index].fork_time[i] - g_process_monitor_list[process_index].fork_time[i - 1];

            min_fork_dif = MIN_FORK_DIF_TIME;

            if (fork_dif > min_fork_dif)
            {
                fork_too_fast = 0;
                break;
            }
        }

        if (fork_too_fast)
        {
            printf("process: %d fork too fast, need restart!!!!!!\n", process_index);
            return -1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        printf("process: %d, fork_index: %d, fork_time: %f\n",
               process_index,
               fork_index,
               fork_time);

        g_process_monitor_list[process_index].fork_time[fork_index] = fork_time;
        g_process_monitor_list[process_index].fork_times++;

        return 0;
    }
}

void sighandel(int sig)
{
    if (!bExit)
    {
        printf("\033[31;1;5m####INFO sigterm\033[0m\n");
        bExit = 1;
    }
    if (bEnd)
    {
        printf("\033[31;1;5m####INFO sigterm End\033[0m\n");
        exit(0);
    }
}

void init_signals(void)
{
    sigset_t signal_mask;
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);

    struct sigaction sa;
    sa.sa_flags = 0;

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGSEGV);

    sa.sa_handler = sighandel;
    sigaction(SIGTERM, &sa, NULL);

    sa.sa_handler = sighandel;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = sighandel;
    sigaction(SIGSEGV, &sa, NULL);
}

void st_log(const char *pLogOut)
{
    printf(pLogOut);
}
int get_process_pid(char *process_name)
{
    if (run_flag & NO_SYSCALL)
    {
        DIR *dir;
        struct dirent *d;
        int pid, i;
        char *s;
        int pnlen;

        i = 0;
        int foundpid[16] = {0};
        pnlen = strlen(process_name);

        /* Open the /proc directory. */
        dir = opendir("/proc");
        if (!dir)
        {
            printf("cannot open /proc");
            return -1;
        }

        /* Walk through the directory. */
        while ((d = readdir(dir)) != NULL)
        {

            char exe[PATH_MAX + 1];
            char path[PATH_MAX + 1];
            int len;
            int namelen;

            /* See if this is a process */
            if ((pid = atoi(d->d_name)) == 0)
                continue;

            snprintf(exe, sizeof(exe), "/proc/%s/exe", d->d_name);
            if ((len = readlink(exe, path, PATH_MAX)) < 0)
                continue;
            path[len] = '\0';

            /* Find process_name */
            s = strrchr(path, '/');
            if (s == NULL)
                continue;
            s++;

            /* we don't need small name len */
            namelen = strlen(s);
            if (namelen < pnlen)
                continue;

            if (!strncmp(process_name, s, pnlen))
            {
                /* to avoid subname like search proc tao but proc taolinke matched */
                if (s[pnlen] == ' ' || s[pnlen] == '\0')
                {
                    foundpid[i] = pid;
                    i++;
                }
            }
        }

        foundpid[i] = 0;
        closedir(dir);

        return foundpid[0];
    }
    else
    {
        char pid_str[256] = "";
        char cmd_str[256];

        sprintf(cmd_str, "ps|grep %s|grep -v grep|grep -v sh|awk \'{print $1}\'",
                process_name);

        // printf("cmd_str: %s\n", cmd_str);

        FILE *fp = popen(cmd_str, "r");
        if (fp == NULL)
        {
            // printf("popen: %s return NULL.\n", cmd_str);
            return 0;
        }
        else
        {
            int flen = fread(pid_str, 1, 256, fp);
            pclose(fp);

            if (flen > 0)
            {
                pid_str[flen] = 0;
                // printf("pid_str: %s\n", pid_str);

                return atoi(pid_str);
            }
            else
                return 0;
        }
    }
}
int main(int argc, char **argv)
{
    init_signals();
    process_monitor_init();
    // fork anjcam process
    printf("====================================fork anjcam start\n");
    pid_t pid_sys = fork();
    if (pid_sys > 0)
    {
        process_fork_and_check(PROCESS_APP);
        printf("fork anjcam ok, pid=%d, %d\n", pid_sys, get_process_pid("anjcam"));
    }
    else if (pid_sys == 0)
    {
        execl("/bin/sh", "sh", "-c", ANJ_APP, (char *)0);
        exit(0);
    }
    else
    {
        printf("fork anjcam failed, errstr= %s \n", strerror(errno));
    }

    sleep(5);
    printf("sys_daemon loop ......\n");
    while (!bExit)
    {
        if (access(DEBUG_DAEMON_LOG, F_OK) == 0)
        {
            // printf("sys_daemon enable console\n");
            freopen("/dev/console", "w", stdout);
            freopen("/dev/console", "w", stderr);
            freopen("/dev/console", "r", stdin);
        }
        else
        {
            // redirect printf to console
            // printf("sys_daemon disable console\n");
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);
            freopen("/dev/console", "r", stdin);
        }
        // check anjcam exit or not
        if (waitpid(pid_sys, (int *)0, WNOHANG) != 0)
        {
            printf("anjcam exit ,fork it again!\n");

            if (0 > process_fork_and_check(PROCESS_APP))
            {
                printf("fork anjcam too fast, system need to be restarted or upgrade!!!!!!\n");

                system("killall anjcam");

                system("touch /tmp/upgrade_only.flag");

                process_monitor_init();
                sleep(3);

                continue;
            }

            system("killall anjcam");

            pid_sys = fork();
            if (pid_sys > 0)
            {
                printf("fork anjcam ok\n");
            }
            else if (pid_sys == 0)
            {
                execl("/bin/sh", "sh", "-c", ANJ_APP, (char *)0);
                exit(0);
            }
            else
            {
                printf("fork anjcam failed, errstr= %s \n", strerror(errno));
            }
        }

        sleep(1);
    }
    printf("########## Exit\n");

    bEnd = 1;
    exit(0);
    return 0;
}
