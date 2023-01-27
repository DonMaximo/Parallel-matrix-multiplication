#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include "matrix.h"
/*void childRoutine(int nbW, Matrix* a, Matrix* b, Matrix* result){
//s 
    //slice M
    //matrixmult and store into slice section of result
    //sem_post();
    //exit(0)
*/
 
void checkError(int status) {
    if (status < 0) {
        printf("error(%s)\n",strerror(errno));
        exit(-1);
    }
}
  
//or should we spawn and store pid in an array so we can specifically reap each child later

/* Convenience function to load a matrix from a file */
Matrix* loadMatrix(char* fName)
{
   FILE* fd = fopen(fName,"r");
   if (fd==NULL) return NULL;
   Matrix* m = readMatrix(fd);
   fclose(fd);
   return m;
}

int main(int argc,char* argv[])
{
    if (argc < 4) {
        printf("usage: mult <file1> <file2> <#workers>\n");
        exit(1);
    }
    Matrix* a = loadMatrix(argv[1]);
    Matrix* b = loadMatrix(argv[2]);
    int nbW = atoi(argv[3]);
    if (a==NULL || b==NULL) {
        printf("Wrong filenames. Couldn't load matrices [%s,%s]\n",argv[1],argv[2]);
        exit(2);
    }

    // TODO

    // Carry out the computation and print the resulting matrix. 
    //LOOK AT SHELL.C FROM HW AGO TO SET UP VARIABLE NUM OF PROCESSES
   
    //set up shared memory zone 
    //slice matrix to divide work for children
    //spawn children and assign them matrices 
    //parent waits until all children finish their work
    //print result
    if (!(a->c == b->r)){
        printf("Multiplication not possible\n");
        return -1;
    }  
        
    if (!nbW){
        Matrix* result = makeMatrix(a->r,b->c);
        result = multMatrix(a,b,result);
        printMatrix(result);
        return 0;
    }
    else 
    {
        const char* zone = "/memzone1";
        int md = shm_open(zone, O_RDWR|O_CREAT,S_IRWXU);
        Matrix* result = makeMatrixMap(zone,md,a->r,b->c);
        sem_t* s = sem_open("/sem1",O_CREAT,0666,0);
        pid_t child;
        int numRows = a->r/nbW, rem = a->r % nbW, i;
        int* childrenRows = (int*)malloc(nbW*sizeof(int));
        for (i =0; i< nbW; i++)
            childrenRows[i] = numRows;
        i =0;
        while (rem){
            childrenRows[i] += 1;
            rem--;
            i++;
        }
        for(i=0; i<nbW; i++){
            if ((child = fork()) == 0){ 
                result = parMultMatrix(nbW,s,a,b,result,childrenRows,i); 
                return 0;
            }
            else if (child < 0){
                printf("fork failed");
                exit(EXIT_FAILURE);
            }
        }
        for (i =0; i< nbW; i++){
            sem_wait(s);
        }
        printMatrix(result);
        free(childrenRows);
        freeMatrixMap(zone,result,md);
    }
    return 0;
}
