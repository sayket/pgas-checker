#include <stdio.h>
#include <stdlib.h>
//#include "open_shmem.h"
#include <shmem.h>

#define N 10

//Global void *
int *GlobalPtr;


void syntheticFunc(int *argPtr, int *privArgPtr)
{
    for (int i = 0; i < N; ++i)
    {
        argPtr[i] = 0;
    }
    shmem_free(argPtr);
    free(privArgPtr);
}

// fails if we try to pass an unsymetric variable to the destination

int main(int argc, char *argv[])
{
    shmem_init();

    
    int *privDest =(int *) malloc(10*sizeof(int));
    int *symDest = (int *) shmem_malloc(10*sizeof(int));
    void *source = (int *) malloc(10*sizeof(int));
    int pe = 1;

    //shmem_put(privDest,source,1,pe);

    //Use the symmetric global pointer GlobalPtr point to dest
    GlobalPtr = privDest;
    
    // boom! the dest is not symmteric our checker catches the error!
    //shmem_put(privDest,source,1,pe);
    //shmem_put(GlobalPtr,source,1,pe);
    
    //Use Private variable(memory) point to symmetric memory
    int *localPtr = symDest;
    shmem_put(&localPtr[0],source,1,pe);
	

    shmem_getmem(source, &symDest[0], 1, pe);


    // Double-free error
    int *mainArgPtr = (int *) shmem_malloc(N*sizeof(int));
    int *privArgPtr = (int *) malloc(N*sizeof(int));
    int *indirectArgPtr = mainArgPtr;
    syntheticFunc(mainArgPtr, privArgPtr);
    free(privArgPtr);
    free(privArgPtr);
    shmem_free(indirectArgPtr);


    free(privDest);
    shmem_free(localPtr);
    // shmem_free(symDest);
    free(source);

    shmem_finalize();
    return 0;
}

