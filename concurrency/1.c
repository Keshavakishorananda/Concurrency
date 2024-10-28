#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"


struct Barista
{
    int baristaindex;
    int free;
};

struct Coffee
{
    char coffee[50];
    int preptime;
};

typedef struct Barista Barista;
typedef struct Coffee Coffee;
struct Costumer
{
    int costumerindex;
    char order[50];
    int baristaindex;
    int preptime;
    int starttime;
    int arrivetime;
    int tolerance;
    time_t start;
    time_t end;
};

typedef struct Costumer Costumer;

sem_t s;
pthread_mutex_t lock;
int array[50];
int waittime = 0;
int waste = 0;

void* threads(void *arg)
{
    Costumer *args = (Costumer*) arg;


    printf(ANSI_COLOR_CYAN "Barista %d begins preparing the order of customer %d at %d second(s)" ANSI_COLOR_RESET "\n",args->baristaindex,args->costumerindex,args->starttime);
    
    int duration = args->preptime;
    if((args->starttime)+(args->preptime)-(args->arrivetime) > args->tolerance)
    {
        duration = (args->arrivetime + args->tolerance) - args->starttime;
        sleep(duration);
        printf(ANSI_COLOR_RED "Customer %d leaves without their order at %d second(s)" ANSI_COLOR_RESET "\n",args->costumerindex,args->arrivetime+args->tolerance + 1);
        sleep(args->preptime - duration);
        printf(ANSI_COLOR_BLUE "Barista %d successfully completes the order of customer %d at %d seconds(s)" ANSI_COLOR_RESET "\n",args->baristaindex,args->costumerindex,args->starttime+args->preptime);
        pthread_mutex_lock(&lock);
        waste = waste + 1;
        pthread_mutex_unlock(&lock);
    }
    else
    {
        sleep(duration);
        printf(ANSI_COLOR_BLUE "Barista %d successfully completes the order of customer %d at %d seconds(s)" ANSI_COLOR_RESET "\n",args->baristaindex,args->costumerindex,args->starttime+args->preptime);
        printf(ANSI_COLOR_GREEN "Customer %d leaves with their order at %d second(s)" ANSI_COLOR_RESET "\n",args->costumerindex,args->starttime+args->preptime);
    }

    pthread_mutex_lock(&lock);
    array[args->baristaindex] = 0;
    sleep(1);
    pthread_mutex_unlock(&lock);

    sem_post(&s);    
}

int main()
{
    int num_baristas;
    int num_coffees;
    int num_costumers;

    scanf("%d %d %d",&num_baristas,&num_coffees,&num_costumers);
    Coffee coffees[num_coffees];
    for(int i=0;i<num_coffees;i++)
    {
        scanf("%s %d",coffees[i].coffee,&coffees[i].preptime);
    }

    Costumer costumer[num_costumers];
    for(int i=0;i<num_costumers;i++)
    {
        scanf("%d %s %d %d",&costumer[i].costumerindex,costumer[i].order,&costumer[i].arrivetime,&costumer[i].tolerance);
    }

    for(int i=0;i<num_costumers;i++)
    {
        for(int k=0;k<num_coffees;k++)
        {
            if(strcmp(costumer[i].order,coffees[k].coffee)==0)
            {
                costumer[i].preptime = coffees[k].preptime;
            }
        }
    }

    for(int i=0;i<=num_baristas;i++)
    {
        array[i] = 0;
    }

    int current_time = 0;
    int j = 0;
    pthread_mutex_init(&lock, NULL);
    sem_init(&s,0,num_baristas);
    pthread_t costumers[num_costumers];

    for(int i=0;i<num_costumers;i++)
    {
        sleep(costumer[i].arrivetime - current_time);
        current_time = costumer[i].arrivetime;
        if(j<=i)
        {
            for(j=i;j<num_costumers;j++)
            {
                if(costumer[i].arrivetime != costumer[j].arrivetime)
                {
                    break;
                }
                else
                {
                    printf("Costumer %d arrives at %d second(s)""\n",costumer[j].costumerindex,costumer[j].arrivetime);
                    printf(ANSI_COLOR_YELLOW "Customer %d orders an %s" ANSI_COLOR_RESET "\n",costumer[j].costumerindex,costumer[j].order);
                    time(&costumer[j].start);
                }
            }
        }
        time_t start,end;
        sem_wait(&s);
        time(&costumer[i].end);
        costumer[i].starttime = costumer[i].arrivetime + (costumer[i].end-costumer[i].start) + 1;

        waittime = waittime + (costumer[i].end-costumer[i].start) + 1;
        pthread_mutex_lock(&lock);
        for(int k=1;k<=num_baristas;k++)
        {
            if(array[k]==0)
            {
                costumer[i].baristaindex = k;
                array[k] = 1;
                break;
            }
        }
        pthread_mutex_unlock(&lock);    
        pthread_create(&costumers[i],NULL,threads,&costumer[i]);
    }

    for(int i = 0; i < num_costumers; i++)
    {
        pthread_join(costumers[i], NULL);
    }

    printf("\n");
    printf("Waiting Time : %f second(s)\n",(float)waittime/num_costumers);
    printf("%d coffee wasted\n",waste);

    pthread_mutex_destroy(&lock);
    sem_destroy(&s);
    return 0;

}