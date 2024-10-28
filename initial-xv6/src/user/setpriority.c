#include "user.h"
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"

int main(int argc, char* argv[])
{
    if(argc != 2)
    {
        printf("Wrong Arguments are given\n");
        exit(1);
    }
    else
    {
        int pid = atoi(argv[0]);
        int new_priority = atoi(argv[1]);

        set_priority(pid, new_priority);
        exit(1);
    }
    return 0;
}