#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<dirent.h>
#include<string.h>
#include<sys/stat.h>
#include<sys/wait.h>

long unsigned recfolder(char* path);
long unsigned handleLink(char* selfPath, char* pointedPath);

int main(int argc, char* argv[])
{
    if(argc == 2 )
    {
        int p[2];
        if(pipe(p) < 0)
        {
            printf("Unable to execute");
            return -1;
        }
        else
        {
            // printf("Entered else ");
            int stdo = dup(1);
            int stdi = dup(0);
            dup2(p[0],0);
            dup2(p[1],1);

            char* CurrPath = argv[1];
            // const pointer towards argv array[1]

            DIR* getdirec = opendir(CurrPath);
            struct dirent* direcP;
            long unsigned TotSize = 0;
            char NewPath[4096];
            char Pathsep[2] = "/";

            int forkcount = 0;
            int* forkPids;
            struct stat sbuf;
            stat(CurrPath, &sbuf);
            TotSize += sbuf.st_size;
            while((direcP = readdir(getdirec)) != NULL)
            {
                if(strcmp(direcP->d_name,".") == 0 || strcmp(direcP->d_name,"..") == 0) continue;

                strcpy(NewPath,CurrPath);
                strcat(NewPath,Pathsep);
                strcat(NewPath,direcP->d_name);                
                //checkIfitsSymbolicLink
                char TmpPath[4096];
                DIR* TmpDir;
                if(readlink(NewPath,TmpPath,4096) == 0 )
                {
                    TotSize += handleLink(NewPath,TmpPath);
                }
                else if((TmpDir = opendir(NewPath)) != NULL)
                {
                    closedir(TmpDir);
                    forkcount++;
                    // ignoring error in fork now
                    int pid = fork();

                    if(pid == 0)
                    {
                        // child will now its subdirectries and files by rec
                        long unsigned size = 0;
                        size += recfolder(NewPath);
                        printf("%lu\n", size);
                        exit(0);
                    }
                    else{
                        forkPids = (int*)malloc(sizeof(int));
                        forkPids[forkcount - 1] = pid;
                        struct stat sbuf;
                        stat(NewPath, &sbuf);
                        TotSize += sbuf.st_size;
                    }
                }
                else{
                    struct stat sbuf;
                    if(stat(NewPath, &sbuf) != 0)
                    {
                    	write(stdo,"Unable to execute",17);
                    	return -1;
                    }
                    TotSize += sbuf.st_size;
                }
            }
            for(int i = 0; i < forkcount; i++)
            {
                int *statloc;
                waitpid(forkPids[i],statloc,0);
            }
            for(int i = 0; i < forkcount; i++)
            {
                long unsigned size;
                scanf("%lu",&size);
                TotSize += size;
            }

            dup2(stdi,0);
            dup2(stdo,1);
            printf("%lu\n", TotSize);
            // catch from pipe the sizes
        }
    }
    else{
        printf("Unable to execute");
        return -1;
    }   
    return 0;
}


long unsigned handleLink(char* selfPath, char* pointedPath)
{
    char TmpPath[4096];
    if(readlink(pointedPath,TmpPath,4096) == 0 )
    {
        return handleLink(pointedPath,TmpPath);
    }
    else{
        // THEN POINTED PATH EITHER A DIRECTORY OR FILE
        DIR* TmpDir;
        long unsigned size = 0;
        if((TmpDir = opendir(pointedPath)) != NULL)
        {
            // folder hai
            size += recfolder(pointedPath);
        }
        // self size
        struct stat sbuf;
        stat(pointedPath, &sbuf);
        return size + sbuf.st_size;        
    }
        

}
long unsigned recfolder(char* path)
{
    DIR* dir = opendir(path);
    // ignoring error as we invoked this function only when path was correct;

    struct dirent* direcent;
    long unsigned size = 0;
    char NewPath[4096];
    char Pathsep[2] = "/";
    while((direcent = readdir(dir)) != NULL)
    {
        if(strcmp(direcent->d_name,".") == 0 || strcmp(direcent->d_name,"..") == 0) continue;
        strcpy(NewPath,path);
        strcat(NewPath,Pathsep);
        strcat(NewPath,direcent->d_name);

        
        //checkIfitsSymbolicLink
        char TmpPath[4096];
        DIR* TmpDir;
        if(readlink(NewPath,TmpPath,4096) == 0 )
        {
            size += handleLink(NewPath,TmpPath);
        }
        else if((TmpDir = opendir(NewPath)) != NULL)
        {
            closedir(TmpDir);
            struct stat sbuf;
            stat(NewPath, &sbuf);
            size += sbuf.st_size;
            size += recfolder(NewPath);
        }
        else{
            struct stat sbuf;
            stat(NewPath, &sbuf);
            size += sbuf.st_size;
        }
    }
    return size;
}
