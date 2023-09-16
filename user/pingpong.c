#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        fprintf(2, "invalid arguments\n");
        exit(1);
    }
    int pids[2];
    char ping_pong[4];

    pipe(pids);

    if (fork() == 0)
    {
        if (read(pids[0], ping_pong, 4))
        {
            printf("%d: received %s\n", getpid(), ping_pong);
        }
        write(pids[1], "pong", 4);
        exit(0);
    }
    else
    {
        write(pids[1], "ping", 4);
        wait(0);
        if (read(pids[0], ping_pong, 4))
        {
            printf("%d: received %s\n", getpid(), ping_pong);
        }
        close(pids[0]);
        close(pids[1]);
    }

    exit(0);
}
