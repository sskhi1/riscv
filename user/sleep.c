#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("invalid command - enter only time in millis\n");
        exit(1);
    }
    int time = atoi(argv[1]);
    sleep(time);

    exit(0);
}
