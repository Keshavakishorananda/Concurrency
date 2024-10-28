#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <stdbool.h>

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_ORANGE "\033[0;33m"
#define ANSI_COLOR_RESET "\x1b[0m"

#define MAX_COSTUMERS 50
#define MAX_MACHINES 50

int num_machines;

struct Machines
{
    int Machindex;
    int starttime;
    int endtime;
    bool status;   // costumer whether allocated or not for that machine
    int situation; // machine is whether started (or) running (or) over
};

struct Flavour
{
    char flavour[50];
    int stand_preptime;
    int fla_index;
};

struct Toppings
{
    char topping[50];
    int stock;
    int top_index;
};

struct Order
{
    int order_index;
    char ice_flav[50];
    int pretime;
    int num_top;
    struct Toppings **toppis;
    int status;
    int mach_no;
    int cust_no;
};

struct Costumer
{
    int costumer_index;
    int entrytime;
    int num_orders;
    struct Order *orders;
    bool pass;
    int status;
};

time_t start;
time_t current;

typedef struct Machines Machines;
typedef struct Flavour Flavour;
typedef struct Toppings Toppings;
typedef struct Order Order;
typedef struct Costumer Costumer;

Machines machines[MAX_MACHINES];
Costumer costumers[MAX_COSTUMERS];
pthread_t machine_thread[MAX_MACHINES];
int cust_num = 0;
int temp = 1;

sem_t max, indicator, costumer_sem;
pthread_mutex_t time_lock;
pthread_mutex_t temp_lock;
pthread_mutex_t new_lock;

void *start_process(void *arg)
{
    int k = 0;
    for (;;)
    {
        if (k == num_machines)
        {
            break;
        }
        for (int i = 0; i < num_machines; i++)
        {

            pthread_mutex_lock(&time_lock);
            time(&current);
            pthread_mutex_unlock(&time_lock);

            if ((current - start) >= machines[i].starttime && machines[i].situation == -1)
            {
                k++;
                pthread_mutex_lock(&time_lock);
                time(&current);
                pthread_mutex_unlock(&time_lock);
                machines[i].status = 0;
                machines[i].situation = 0;
                printf(ANSI_COLOR_ORANGE "Machine %d has started working at %d second(s)" ANSI_COLOR_RESET "\n", i + 1, (int)(current - start));
                sem_post(&indicator);
            }
        }
        sleep(1);
    }
}

void *stop_process(void *arg)
{
    int k = 0;
    for (;;)
    {
        if (k == num_machines)
            break;
        sleep(1);
        pthread_mutex_lock(&time_lock);
        time(&current);
        pthread_mutex_unlock(&time_lock);
        for (int i = 0; i < num_machines; i++)
        {

            if ((current - start) >= machines[i].endtime && machines[i].situation == 0)
            {
                k++;
                printf("\e[38;2;255;85;0mMachine %d has stopped working at %d second(s)\e[0;37m\n", i + 1, (int)(current - start));
                machines[i].situation = 1;
            }
        }
    }
}

void *preperation(void *arg)
{
    Order *args = (Order *)arg;
    pthread_mutex_lock(&time_lock);
    time(&current);
    pthread_mutex_unlock(&time_lock);
    printf(ANSI_COLOR_CYAN "Machine %d starts preparing ice cream %d of customer %d at %d second(s)" ANSI_COLOR_RESET "\n", (args->mach_no + 1), args->order_index, args->cust_no + 1, (int)(current - start));
    for (int some_time = args->pretime - 1; some_time > 0; some_time--)
    {
        sleep(1);
        pthread_mutex_lock(&time_lock);
        time(&current);
        pthread_mutex_unlock(&time_lock);
    }
    for (int i = 0; i < args->num_top; i++)
    {
        if (args->toppis[i]->stock < 0 && args->toppis[i]->stock != -1)
        {
            args->toppis[i]->stock = 0;
        }
        else if (args->toppis[i]->stock > 0)
        {
            args->toppis[i]->stock = args->toppis[i]->stock - 1;
        }
    }
    pthread_mutex_lock(&time_lock);
    time(&current);
    pthread_mutex_unlock(&time_lock);
    printf(ANSI_COLOR_BLUE "Machine %d completes preparing ice cream %d of customer %d at %d seconds(s)" ANSI_COLOR_RESET "\n", args->mach_no + 1, args->order_index, args->cust_no + 1, (int)(current - start));
    machines[args->mach_no].status = 0;
    args->status = 2;
}

void check(Costumer *check_costumer)
{
    if (check_costumer->status == 1 && check_costumer->pass == 1)
    {
        for (int k = 0; k < check_costumer->num_orders; k++)
        {
            if (check_costumer->pass == 1 && check_costumer->orders[k].status == 1)
            {
                for (int l = 0; l < check_costumer->orders[k].num_top; l++)
                {
                    if (check_costumer->orders[k].toppis[l]->stock <= 0 && check_costumer->orders[k].toppis[l]->stock != -1)
                    {
                        check_costumer->pass = 0;
                        break;
                    }
                }
            }
        }
    }
}

void *allow_process(void *arg)
{
    sem_wait(&indicator);    // waiting for customer to get orders
    sem_wait(&costumer_sem); // for customer to print orders
    for (;;)
    {
        int leave = 0;
        for (int i = 0; i < num_machines; i++)
        {
            if (machines[i].situation == 0)
            {

                if (machines[i].status == 0)
                {
                    for (int j = 0; j < cust_num; j++)
                    {
                        check(&costumers[j]);
                        if (costumers[j].pass == 1)
                        {
                            for (int k = 0; k < costumers[j].num_orders; k++)
                            {
                                pthread_mutex_lock(&new_lock);
                                pthread_mutex_lock(&time_lock);
                                time(&current);
                                pthread_mutex_unlock(&time_lock);
                                if (costumers[j].orders[k].pretime <= machines[i].endtime - (current - start))
                                {
                                    if (machines[i].status == 0 && costumers[j].orders[k].status == 1)
                                    {
                                        if (machines[i].situation == 0 && costumers[j].status == 1)
                                        {
                                            machines[i].status = 1;
                                            costumers[j].orders[k].cust_no = j;
                                            sleep(1);
                                            costumers[j].orders[k].mach_no = i;
                                            costumers[j].orders[k].status = 0;
                                            pthread_mutex_lock(&time_lock);
                                            time(&current);
                                            pthread_mutex_unlock(&time_lock);
                                            pthread_create(&machine_thread[i], NULL, preperation, &costumers[j].orders[k]);
                                        }
                                    }
                                }
                                pthread_mutex_unlock(&new_lock);
                            }
                        }
                        else if (costumers[j].status != 2)
                        {
                            pthread_mutex_lock(&temp_lock);
                            pthread_mutex_lock(&time_lock);
                            time(&current);
                            pthread_mutex_unlock(&time_lock);
                            printf(ANSI_COLOR_RED "Customer %d left at %d second(s) with an unfulfilled order" ANSI_COLOR_RESET "\n", costumers[j].costumer_index, (int)(current - start));
                            sem_post(&max);
                            pthread_mutex_unlock(&temp_lock);
                            costumers[j].status = 2;
                        }
                    }
                }
            }
        }
        pthread_mutex_lock(&time_lock);
        time(&current);
        pthread_mutex_unlock(&time_lock);
        int count;
        for (int w = 0; w < cust_num; w++)
        {
            int i = 0;
            pthread_mutex_lock(&temp_lock);
            while (i < costumers[w].num_orders)
            {
                if (costumers[w].orders[i].status != 2)
                {
                    break;
                }
                i++;
            }
            if (costumers[w].status == 2)
                count++;

            if (costumers[w].status != 2 && i == costumers[w].num_orders)
            {
                printf(ANSI_COLOR_GREEN "Customer %d has collected their order(s) and left at %d second(s)" ANSI_COLOR_RESET "\n", costumers[w].costumer_index, (int)(current - start));
                sem_post(&max);
                costumers[w].status = 2;
            }
            if (cust_num == count)
            {
                temp = 0;
                break;
            }
            pthread_mutex_unlock(&temp_lock);
        }
        for (int i = 0; i < num_machines; i++)
        {
            if (machines[i].situation == 1)
                leave++;
            if (leave == num_machines)
            {
                leave = -1;
                break;
            }
        }
        if (leave == -1)
        {
            break;
        }
        if(temp == 0){
            break;
        }
    }
}

int main()
{
    int max_costumers, num_flav, num_toppings;

    scanf("%d %d %d %d", &num_machines, &max_costumers, &num_flav, &num_toppings);

    Flavour flavour[num_flav];
    Toppings toppings[num_toppings];

    for (int i = 0; i < num_machines; i++)
    {
        machines[i].status = -1;
        machines[i].Machindex = i + 1;
        scanf("%d", &machines[i].starttime);
        scanf("%d", &machines[i].endtime);
        machines[i].situation = -1;
    }

    for (int i = 0; i < num_flav; i++)
    {
        flavour[i].fla_index = i + 1;
        scanf("%s %d", flavour[i].flavour, &flavour[i].stand_preptime);
    }

    for (int i = 0; i < num_toppings; i++)
    {
        toppings[i].top_index = i;
        scanf("%s %d", toppings[i].topping, &toppings[i].stock);
    }

    getchar();
    while (max_costumers > 0)
    {
        char line_1[20];
        fgets(line_1, 20, stdin);
        if ((line_1[1] == '\0' && line_1[0] == '\n'))
        {
            break;
        }

        costumers[cust_num].costumer_index = cust_num + 1;
        sscanf(line_1, "%d %d %d", &costumers[cust_num].costumer_index, &costumers[cust_num].entrytime, &costumers[cust_num].num_orders);
        int total_orders = costumers[cust_num].num_orders;
        costumers[cust_num].pass = 1;
        costumers[cust_num].orders = (Order *)malloc(costumers[cust_num].num_orders * sizeof(Order));
        for (int i = 0; i < total_orders; i++)
        {
            char input[1000] = {'\0'};
            char *string;
            fgets(input, sizeof(input), stdin);
            if (input[strlen(input) - 1] == '\n')
            {
                input[strlen(input) - 1] = '\0';
            }

            string = strtok(input, " ");
            strcpy(costumers[cust_num].orders[i].ice_flav, string);
            // printf("%s\n",costumers[cust_num].orders[i].ice_flav);
            for (int k = 0; k < num_flav; k++)
            {
                if (!strcmp(flavour[k].flavour, costumers[cust_num].orders[i].ice_flav))
                {
                    costumers[cust_num].orders[i].pretime = flavour[k].stand_preptime;
                    break;
                }
            }
            costumers[cust_num].status = 0;
            costumers[cust_num].orders[i].order_index = i + 1;

            string = strtok(NULL, " ");
            costumers[cust_num].orders[i].toppis = (struct Toppings **)malloc(num_toppings * sizeof(struct Toppings *));
            for (int j = 0; j < num_toppings; j++)
            {
                costumers[cust_num].orders[i].toppis[j] = (Toppings *)malloc(sizeof(Toppings));
            }
            costumers[cust_num].orders[i].status = 1;
            int num = 0;
            while (string != NULL)
            {
                strcpy(costumers[cust_num].orders[i].toppis[num]->topping, string);
                for (int j = 0; j < num_toppings; j++)
                {
                    if (strcmp(costumers[cust_num].orders[i].toppis[num]->topping, toppings[j].topping) == 0)
                    {
                        costumers[cust_num].orders[i].toppis[num]->top_index = toppings[j].top_index;
                        costumers[cust_num].orders[i].toppis[num]->stock = toppings[j].stock;
                        // printf("%d %s",costumers[cust_num].orders[i].toppis[num]->stock);
                        break;
                    }
                }
                string = strtok(NULL, " ");
                num++;
            }
            costumers[cust_num].orders[i].num_top = num;
        }
        cust_num = cust_num + 1;
    }

    pthread_t start_machine;
    pthread_t stop_machine;
    pthread_t allow_thread;

    sem_init(&max, 0, max_costumers);
    sem_init(&indicator, 0, 0);
    sem_init(&costumer_sem, 0, 0);

    pthread_mutex_init(&time_lock, NULL);

    time(&start);
    pthread_create(&start_machine, NULL, start_process, NULL);
    pthread_create(&stop_machine, NULL, stop_process, NULL);
    pthread_create(&allow_thread, NULL, allow_process, NULL);

    pthread_mutex_lock(&time_lock);
    time(&current);
    pthread_mutex_unlock(&time_lock);

    for (int i = 0; i < cust_num; i++)
    {
        pthread_mutex_lock(&time_lock);
        time(&current);
        pthread_mutex_unlock(&time_lock);

        sem_wait(&max);

        pthread_mutex_lock(&time_lock);
        time(&current);
        pthread_mutex_unlock(&time_lock);

        while ((current - start) < costumers[i].entrytime)
        {
            sleep(1);
            pthread_mutex_lock(&time_lock);
            time(&current);
            pthread_mutex_unlock(&time_lock);
        }

        printf("Customer %d enters at %d second(s)\n", costumers[i].costumer_index, (int)(current - start));
        printf(ANSI_COLOR_YELLOW "Customer %d orders %d ice creams" ANSI_COLOR_RESET "\n", costumers[i].costumer_index, costumers[i].num_orders);
        pthread_mutex_lock(&time_lock);
        time(&current);
        pthread_mutex_unlock(&time_lock);
        for (int j = 0; j < costumers[i].num_orders; j++)
        {
            int k = 0;
            printf(ANSI_COLOR_YELLOW "Ice cream %d: %s " ANSI_COLOR_RESET, j + 1, costumers[i].orders[j].ice_flav);
            while (k < costumers[i].orders[j].num_top)
            {
                printf(ANSI_COLOR_YELLOW "%s " ANSI_COLOR_RESET, costumers[i].orders[j].toppis[k]->topping);
                k++;
            }
            printf("\n");
            sem_post(&costumer_sem);
        }
        costumers[i].status = 1;
    }

    pthread_join(allow_thread, NULL);
    pthread_join(start_machine, NULL);
    for (int i = 0; i < num_machines; i++)
    {
        pthread_join(machine_thread[i], NULL);
    }
    pthread_join(stop_machine, NULL);
    for (int i = 0; i < cust_num; i++)
    {
        if (costumers[i].status != 2)
        {
            printf(ANSI_COLOR_RED "Customer %d was not serviced due to unavailability of machines" ANSI_COLOR_RESET "\n", i + 1);
        }
    }

    printf("Parlour Closed\n");

    return 0;
}