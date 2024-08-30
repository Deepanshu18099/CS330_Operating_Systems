#include<math.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
int main(int argc, char* argv[])
{
    char** ptr;

    unsigned long operand = strtoul(argv[argc - 1], ptr, 10);
    free(ptr);
    if(argc == 2)
    {
        printf("%lu\n", operand*operand);
        return 0;
    }
    else
    {
        // take one operation and pass to next function through exec
        // take operand
        sprintf(argv[argc - 1], "%lu", operand*operand);
        // make new arguments and argument length for next call of exec

        // replace number at the last position of this array
        char** newargv = argv + 1;
        // we got arguments array of strings started from 2nd element of array;

        char* operand = argv[1];
        // assuming no error
        execv(newargv[0], newargv);

        printf("Unable to execute");
        return -1;
    }
}