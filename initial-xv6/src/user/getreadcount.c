#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Reference: https://cs631.cs.usfca.edu/guides/adding-a-syscall-to-xv6

int
main(int argc, char *argv[])
{
    printf("Current read count is %d\n", getreadcount());
    exit(0);
}