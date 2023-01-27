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
#include <ctype.h>

/*
#include "matrix.h"
#include <unistd.h>
#include <errno.h>
#include <string.h>
*/
size_t sizeMatrix(int r,int c)
{
   /*
    * returns the nummber of bytes needed to hold a matrix of r x c values (type int)
    */
    return (sizeof(int)*r*c);
}

Matrix* makeMatrix(int r,int c)
{
   /*
    * allocates space to hold a matrix of r x c values (type int)
    */
    Matrix* m = (Matrix*)malloc(sizeof(Matrix)+sizeMatrix(r,c));
    m->r = r;
    m->c =c;
    return m;
}

Matrix* makeMatrixMap(void* zone,int md, int r,int c)
{
   /*
    * Allocates space for a matrix of r x c from a _given_ memory block at address zone. 
    */
    size_t sz = sizeof(Matrix)+sizeMatrix(r,c); //DO I NEED THIS?
    ftruncate(md,sz);
    Matrix* m = mmap(NULL,sz,PROT_READ|PROT_WRITE,MAP_SHARED,md,0);
    if (m == MAP_FAILED){
        printf("mmap failed: %s\n",strerror(errno));
        exit(1);
    }
    m->r =r;
    m->c =c;
    return m;
}

void freeMatrixMap(void* zone, Matrix* m, int md){
    munmap(m,sizeof(Matrix)+sizeMatrix(m->r,m->c));
    close(md);
    shm_unlink(zone);
}
/**
 * Reads a single integer from the FILE fd
 * output: a single integer.
 * Note: the function uses getc_unlocked to _not_ pay the overhead of locking and
 * unlocking the file for each integer to be read (imagine reading a matrix of 1000x1000,
 * that's 1,000,000 calls to getc and therefore 1,000,000 calls to locking the file. 
 */
int readValue(FILE* fd)
{
   int v = 0;
   char ch;
   int neg=1;
   while (((ch = getc_unlocked(fd)) != EOF) && isspace(ch)); // skip WS.      
   while (!isspace(ch)) {
      if (ch=='-')
         neg=-1;
      else
         v = v * 10 + ch - '0';
      ch = getc_unlocked(fd);
   }
   return neg * v;
}

Matrix* readMatrix(FILE* fd)
{
   /*
    * Read a matrix from a file (fd). The matrix is held in a text format that first conveys
    * the number of rows and columns, then, each row is on a line of text and all the 
    * values are separated by white spaces. 
    * Namely:
    * r c
    * v0,0 v0,1 .... v0,c-1
    * v1,0 v1,1 .... v1,c-1
    * ....
    * vr-1,0 ....    v_r-1,c-1
    */
   int r,c,v;
   int nv = fscanf(fd,"%d %d",&r,&c);
   Matrix* m = makeMatrix(r,c);
   flockfile(fd);
   for(int i=0;i < r;i++)
      for(int j=0;j < c;j++) {
         v = readValue(fd);
         M(m,i,j) = v;
      }
   funlockfile(fd);
   return m;
}


void freeMatrix(Matrix* m)
{
   /*
    * deallocates the space used by matrix m
    */
    free(m);
}
void printMatrix(Matrix* m)
{
   /*
    * Print the matrix on the sandard output. One row per line, values for the row
    * separated by white spaces. 
    */
    for (int i =0; i< m->r; i++){
        for (int j=0; j< m->c; j++)
            printf("%d ",M(m,i,j));
        printf("\n");
    }
    printf("\n");
}

Matrix* multMatrix(Matrix* a,Matrix* b,Matrix* into)
{  // mxn * nxp yields an mxp matrix
   /*
    * Compute the produce of A * B and store the result in `into`.
    * The computation is sequential and is only meant to be used as a
    * check for your parallel code. 
    *
    * Return value: the matrix `into` were the result is held.
    */
    int rowsA = a->r;
    int rowsB = b->r;
    int colsB = b->c;
    int i,j,k;
    int sum =0;
    for (i = 0; i < rowsA; i++){
        for(j = 0; j < colsB; j++){
            for(k = 0; k < rowsB; k++){
                sum = sum + M(a,i,k)*M(b,k,j);
            }
            M(into,i,j) = sum;
            sum =0;       
        }
    }
    return into;
}

Matrix* parMultMatrix(int nbW,sem_t* sem,Matrix* a,Matrix* b,Matrix* into, int* childRows, int childNbr)
{
   /*
    * Compute the produce of A * B and store the result in `into`
    * The compuration is done in parallel with nbW worker **processes** (NOT threads). 
    * It should have the same output as the sequential version, but faster. ;-)
    * How you use the semaphore is up to you (as long as you use it!)
    * You CANNOT use the wait system call. 
    *
    * Return value: the matrix `into` were the result is held.
    */
    //
    int rowStart, rowEnd, rowsB = b->r, i, j, k, colsB = b->c,sum = 0;
    if (childNbr ==0){
        rowStart = childNbr;
        rowEnd = childRows[childNbr];
    }
    else {
        rowStart =0;
        for (int v=0; v<= childNbr-1; v++){
            rowStart += childRows[v];
        }
        rowEnd = rowStart+ childRows[childNbr];
    }
    for (i = rowStart; i < rowEnd; i++){
        for(j = 0; j < colsB; j++){
            for( k =0; k < rowsB; k++){
                sum = sum + M(a,i,k)*M(b,k,j);
            }
            M(into,i,j) = sum;
            sum = 0;
        }
    }
    sem_post(sem);
    return into; 
}    
