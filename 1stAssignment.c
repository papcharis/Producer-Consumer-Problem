/*
* FILE: prod-cons.c
* THMMY, 8th semester, Real Time Embedded Systems: 1st assignment
* Parallel implementation of producer - consumer problem
* Author:
*   Papadakis Charalampos, 9128, papadakic@ece.auth.gr
*/


#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

#define QUEUESIZE 500
#define LOOP 400000 //the number of workFunction objects each producer will create
// #define P  16 //number of Producers
// #define Q  128 //number of Consumers

#define noFunctions 5 //number of work functions
#define noArguments 10 //number of random arguments

#define PI 3.14159265

//FILE * fp ;

void *producer (void *arg);
void *consumer (void *arg);

void * printSinX(void * arg);
void * printCosX( void * arg);
void * squareX(void *arg);
void * squareRootX(void *arg);
void * logX(void *arg);

void * (*functions[noFunctions]) (void * ) = { printSinX  , printCosX , squareX  , squareRootX , logX};

int arguments[noArguments]={ 0 , 10 , 20 , 30 , 40 , 50 , 75 , 100 , 125 , 150};

int P; //number of producers - command line argument
int Q; //number of consumers - command line argument

typedef struct {
  void * (*work)(void *);
  void * arg;
} workFunction ;

struct timeval arrTime[QUEUESIZE];


int doneProducers = 0; //counter for how many producers have completed their job
struct timeval timeStamp;
double tempTime = 0;

typedef struct {
  workFunction  buf[QUEUESIZE];
  long head, tail;
  int full, empty;
  pthread_mutex_t *mut;
  pthread_cond_t *notFull, *notEmpty;
} queue;


queue *queueInit (void);
void queueDelete (queue *q);
void queueAdd (queue *q, workFunction in);
void queueDel (queue *q, workFunction *out);

void *producer (void *q){
  queue *fifo;
  int i;
  fifo = (queue *)q;
  int randomFunc  , randomArg ;


  for (i = 0; i < LOOP; i++) {
    pthread_mutex_lock (fifo->mut);

    while (fifo->full) {
      pthread_cond_wait (fifo->notFull, fifo->mut);
    }
    workFunction wf ;
    randomFunc = rand()%noFunctions; //chooses one of the 5 work functions randomly
    wf.work = functions[randomFunc];
    randomArg = rand()%noArguments; //chooses one of the 10 arguments randomly
    wf.arg = &arguments[randomArg];

    gettimeofday(&(arrTime[(fifo->tail)]),NULL);//the moment when the object is added to the queue
    queueAdd (fifo, wf);
    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notEmpty);
  }

  doneProducers++;

  if(doneProducers == P) {
    pthread_cond_broadcast(fifo->notEmpty);
  }

  return (NULL);
}



void *consumer (void *q)
{

  queue *fifo;
  int i, d;
  fifo = (queue *)q;
  double waitingTime;


    while(1) {

      pthread_mutex_lock (fifo->mut);

      while(fifo->empty==1 && doneProducers!=P) {
        pthread_cond_wait (fifo->notEmpty, fifo->mut);
      }
      //when the fifo is empty and all the producers completed their job its time for the consumers to stop working
      if(doneProducers==P && fifo->empty==1) {
        pthread_mutex_unlock (fifo->mut);
        pthread_cond_broadcast(fifo->notEmpty);//frees the consumer from the pthread_cond_wait condition
        break;
      }

      workFunction wf ;
      struct timeval leaveTime ;
      gettimeofday(&leaveTime,NULL);
      waitingTime= (double)(leaveTime.tv_sec -(arrTime[fifo->head]).tv_sec) *1e6 + (leaveTime.tv_usec-(arrTime[fifo->head]).tv_usec) ;
      queueDel (fifo, &wf);//consumer gets an object and is ready to execute the work function of it
      // printf("%lf\n",waitingTime);
      tempTime += waitingTime;

      pthread_mutex_unlock (fifo->mut);
      pthread_cond_signal (fifo->notFull);

      wf.work(wf.arg);//consumer executes the work function with the right argument
    }

    return (NULL);

}


queue *queueInit (void)
{
  queue *q;

  q = (queue *)malloc (sizeof (queue));
  if (q == NULL) return (NULL);

  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;
  q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (q->mut, NULL);
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notEmpty, NULL);

  return (q);
}

void queueDelete (queue *q)
{
  pthread_mutex_destroy (q->mut);
  free (q->mut);
  pthread_cond_destroy (q->notFull);
  free (q->notFull);
  pthread_cond_destroy (q->notEmpty);
  free (q->notEmpty);
  free (q);
}

void queueAdd (queue *q,workFunction in)
{
  q->buf[q->tail] = in;
  q->tail++;
  if (q->tail == QUEUESIZE)
    q->tail = 0;
  if (q->tail == q->head)
    q->full = 1;
  q->empty = 0;

  return;
}

void queueDel (queue *q, workFunction *out)
{
  *out = q->buf[q->head];

  q->head++;
  if (q->head == QUEUESIZE)
    q->head = 0;
  if (q->head == q->tail)
    q->empty = 1;
  q->full = 0;

  return;
}



int main (int argc , char *argv[])
{
  if (argc==3){
  srand(time(NULL));
  P=atoi(argv[1]);//number of producers - command line argument
  Q=atoi(argv[2]);//number of consumers - command line argument
  queue *fifo;
  pthread_t producers[P], consumers[Q];
  fifo = queueInit ();

  if (fifo ==  NULL) {
    fprintf (stderr, "main: Queue Initialization failed.\n");
    exit (1);
  }
//creating Q consumer threads
  for(int i=0; i<Q; i++){
    pthread_create(&consumers[i], NULL, consumer, fifo);
}
//creating P producer threads
  for(int i=0; i<P ; i++){
  pthread_create (&producers[i], NULL, producer, fifo);
}

//Waits for all the producer threads to join
for(int i=0; i<P ; i++){
 pthread_join(producers[i], NULL);
}

//Waits for all the consumer threads to join
for(int i=0; i<Q ; i++){
  pthread_join(consumers[i], NULL);
}
  //calculates and prints the average time that takes to an object to be "executed" after being submitted to the list
  double averageTime = ((double)tempTime/(P*LOOP));
  printf("The average waiting time is :  %lf \n" , averageTime);

  queueDelete(fifo);
}
else{
  printf("The program expected two arguments ,one for Producers and one for Consumers\n");
}
  return 0;
}

void * printSinX(void * arg){
  double x = *(int *)arg ;
  double result = sin(x*(PI/180));
  //sleep(0.1);
  //printf("The sine of %lf degrees is : %lf \n",x,result);
}

void * printCosX ( void * arg){
  double x = *(int *)arg ;
  double result = cos(x*(PI/180));
  //sleep(0.1);
  //printf("The cosine of %lf degrees is : %lf \n",x,result);

}

void * squareX(void *arg){
  int x = *(int *)arg ;
  int result = x * x ;
  //sleep(0.1);
  //printf("The square of %d is : %d \n",x,result);

}

void * squareRootX(void *arg){
  double x = *(int *)arg ;
  double result = sqrt(x);
  //sleep(0.1);
  //printf("The square root of %lf is : %lf \n",x,result);
}

void * logX(void *arg){
  double x = *(int *)arg;
  if(x==0){
    //printf("The logarithm of number 0 does not exist");
  }
  else{
    double result=log(x);
    //printf("The logarithm of number %lf using number e as base of the logarithm is: %lf ",x,result);
  }
//  sleep(0.1);
}
