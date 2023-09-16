#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void primesPipeline(int *pids)
{
    int p, tmp, locPids[2];
    close(pids[1]);
    pipe(locPids);
    if (read(pids[0], &p, sizeof(int)) == 4)
    {
        printf("prime %d\n", p);
        if (read(pids[0], &tmp, sizeof(int)) == 4)
        {
            if (fork() == 0)
            {
                primesPipeline(locPids);
            }
            else
            {
                if (tmp % p)
                {
                    write(locPids[1], &tmp, 4);
                }
                while (read(pids[0], &tmp, 4) == 4)
                {
                    if (tmp % p)
                    {
                        write(locPids[1], &tmp, 4);
                    }
                }
                close(locPids[1]);
                wait(0);
                close(locPids[0]);
            }
        }
    }
    else
    {
        printf("Error reading prime number\n");
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        fprintf(2, "invalid arguments\n");
        exit(1);
    }
    int pids[2];
    pipe(pids);

    if (fork() == 0)
    {
        primesPipeline(pids);
    }
    else
    {
        for (int i = 2; i <= 35; i++)
        {
            write(pids[1], &i, sizeof(int));
        }
        close(pids[1]);
        wait(0);
        close(pids[0]);
    }

    exit(0);
}
