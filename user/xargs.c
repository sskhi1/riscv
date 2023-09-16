#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("invalid command - xargs needs at least one argument\n");
        exit(1);
    }
    int argc_x;
    char *argv_x[MAXARG];
    memset(argv_x, 0, sizeof(argv_x));
    char buf[1024];
    struct stat st;

    argc_x = argc - 1;
    for (int i = 1; i < argc; i++)
    {
        argv_x[i - 1] = argv[i];
    }

    if (fstat(0, &st) < 0)
    {
        argv_x[argc_x] = buf;
        memset(buf, 0, sizeof(buf));

        int i = 0;
        while (read(0, buf + i, 1) > 0)
        {
            if (i > 1023)
            {
                printf("Illegal arguments - too long argument\n");
                exit(1);
            }
            if (*(buf + i) != '\n')
            {
                i++;
            }
            else
            {
                buf[i] = 0;
                if (fork() == 0)
                {
                    exec(argv_x[0], argv_x);
                    exit(0);
                }
                else
                {
                    wait(0);
                }

                i = 0;
                memset(buf, 0, sizeof(buf));
            }
        }

        if (buf[0])
        {
            if (fork() == 0)
            {
                exec(argv_x[0], argv_x);
                exit(0);
            }
            else
            {
                wait(0);
            }
        }
    }
    else
    {
        if (fork() == 0)
        {
            exec(argv_x[0], argv_x);
            exit(0);
        }
        else
        {
            wait(0);
        }
    }

    exit(0);
}
