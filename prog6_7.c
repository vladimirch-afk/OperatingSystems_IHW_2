#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <time.h>

pid_t chpid1, chpid2;
int columns;
int rows;
const int MAX_OF_SEMAPHORES = 1024;
const int EMPTY_PLOT_COEFFICIENT = 2;
char *sempahore_template_name = "/garden-semaphore-id-";
const char *shared_object = "/posix-shared-object";
const char *sem_shared_object = "/posix-sem-shared-object";
int main_shmid;
int sem_main_shmid;

struct GTask
{
    int p_i;
    int p_j;
    int gardener_id;
    int work_time;
};

// Определить семафоры
void getSemaphores(sem_t **semaphores, int columns, int rows, int *sem_shmid)
{
    if ((*sem_shmid = shm_open(sem_shared_object, O_RDWR | O_NONBLOCK, 0666)) < 0)
    {
        perror("Can't connect to shared memory");
        exit(-1);
    }
    else
    {
        if ((*semaphores = mmap(0, columns * rows / 4 * sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED, *sem_shmid, 0)) < 0)
        {
            printf("Can't connect to shared memory\n");
            exit(-1);
        };
    }
}

// Получить сад
void aquireField(int **field, int field_size, int *shmid)
{
    if ((*shmid = shm_open(shared_object, O_RDWR | O_NONBLOCK, 0666)) < 0)
    {
        perror("Can't connect to shared memory");
        exit(-1);
    }
    else
    {
        if ((*field = mmap(0, field_size * sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED, *shmid, 0)) < 0)
        {
            printf("Can't connect to shared memory\n");
            exit(-1);
        };
    }
}

void plotGardenn(sem_t *semaphores, int *field, int big_columns, struct GTask task)
{
    sem_wait(semaphores + (task.p_i / 2 * big_columns + task.p_j / 2));
    printf("Gardener %d on row: %d, column: %d\n", task.gardener_id, task.p_i, task.p_j);
    fflush(stdout);
    if (field[task.p_i * columns + task.p_j] == 0)
    {
        field[task.p_i * columns + task.p_j] = task.gardener_id;
        usleep(task.work_time * 1000);
    }
    else
    {
        usleep(task.work_time / EMPTY_PLOT_COEFFICIENT * 1000);
    }
    sem_post(semaphores + (task.p_i / 2 * big_columns + task.p_j / 2));
}

// Запустить первого садовника
void launchFirstGardener(int columns, int rows, int workingTimeMilliseconds)
{
    int big_columns = columns / 2;
    int field_size = columns * rows;
    int shmid;
    int sem_shmid;
    sem_t *semaphores;
    getSemaphores(&semaphores, columns, rows, &sem_shmid);
    printf("Gardener 1 open shared memory with semaphores\n");
    fflush(stdout);
    int *field;
    aquireField(&field, field_size, &shmid);
    printf("Gardener 1 open shared memory with field\n");
    fflush(stdout);
    int i = 0;
    int j = 0;
    struct GTask task;
    task.gardener_id = 1;
    task.work_time = workingTimeMilliseconds;
    while (i < rows)
    {
        while (j < columns)
        {
            task.p_i = i;
            task.p_j = j;
            plotGardenn(semaphores, field, big_columns, task);
            ++j;
        }
        ++i; --j;
        while (j >= 0)
        {
            task.p_i = i;
            task.p_j = j;
            plotGardenn(semaphores, field, big_columns, task);
            --j;
        }
        ++i; ++j;
    }
    printf("Gardener 1 finish work\n");
    exit(0);
}

// Запустить второго садовника
void launchSecondGardener(int columns, int rows, int workingTimeMilliseconds)
{
    int big_columns = columns / 2;
    int field_size = columns * rows;
    int shmid;
    int sem_shmid;
    sem_t *semaphores;
    getSemaphores(&semaphores, columns, rows, &sem_shmid);
    printf("Gardener 2 open shared memory with semaphores\n");
    fflush(stdout);
    int *field;
    aquireField(&field, field_size, &shmid);
    printf("Gardener 2 open shared memory with field\n");
    fflush(stdout);
    int i = rows - 1;
    int j = columns - 1;
    struct GTask task;
    task.gardener_id = 2;
    task.work_time = workingTimeMilliseconds;
    while (j >= 0)
    {
        while (i >= 0)
        {
            task.p_i = i;
            task.p_j = j;
            plotGardenn(semaphores, field, big_columns, task);
            --i;
        }
        --j; ++i;
        while (i < rows)
        {
            task.p_i = i;
            task.p_j = j;
            plotGardenn(semaphores, field, big_columns, task);
            ++i;
        }
        --i; --j;
    }
    printf("Gardener 2 finish work\n");
    exit(0);
}

// Напечатать поле в консоль
void printField(int *field)
{
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < columns; ++j)
        {
            if (field[i * columns + j] < 0)
            {
                printf("X ");
            }
            else
            {
                printf("%d ", field[i * columns + j]);
            }
        }
        printf("\n");
    }
}

// Инициализируем поле
void initializeField(int *field)
{
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < columns; ++j)
        {
            field[i * columns + j] = 0;
        }
    }

    int percentage = 10 + random() % 20;
    int count_of_bad_plots = columns * rows * percentage / 100;
    for (int i = 0; i < count_of_bad_plots; ++i)
    {
        int row_index;
        int column_index;
        do
        {
            row_index = random() % rows;
            column_index = random() % columns;
        } while (field[row_index * columns + column_index] == -1);

        field[row_index * columns + column_index] = -1;
    }
}

// Создать семафоры под каждую клетку сада
void createSemaphores(sem_t *semaphores)
{
    for (int k = 0; k < columns * rows / 4; ++k)
    {
        if (sem_init(semaphores + k, 1, 1) < 0)
        {
            perror("sem_init: can not create semaphore");
            exit(-1);
        };

        int val;
        sem_getvalue(semaphores + k, &val);
        if (val != 1)
        {
            printf("Ooops, one of semaphores can't set initial value to 1. Please, restart program\n");
            shm_unlink(shared_object);
            exit(-1);
        }
    }
}

void keyboard_interruption_handler(int num)
{
    kill(chpid1, SIGINT);
    kill(chpid2, SIGINT);
    printf("Closing resources...\n");
    shm_unlink(shared_object);
    exit(0);
}

int main(int argc, char *argv[])
{
    srand(time(NULL));

    if (argc != 4)
    {
        printf("Invalid count of arguments. "
               "Expected 3 arguments: garden_side_size, first_speed, second_speed\n");
        exit(-1);
    }

    int square_side_size = atoi(argv[1]);
    if (square_side_size * square_side_size > MAX_OF_SEMAPHORES)
    {
        printf("Too large garden_side_size\n");
        exit(-1);
    }
    else if (square_side_size < 2)
    {
        printf("Too small garden_side_size\n");
        exit(-1);
    }

    rows = columns = 2 * square_side_size;
    int field_size = rows * columns;
    int sem_count = field_size / 4;
    int first_gardener_working_time = atoi(argv[2]);
    int second_gardener_working_time = atoi(argv[3]);

    if (first_gardener_working_time < 1 || second_gardener_working_time < 1)
    {
        printf("Working time should be greater than 0\n");
        exit(-1);
    }

    int *field;
    sem_t *semaphores;
    if ((main_shmid = shm_open(shared_object, O_CREAT | O_RDWR, 0666)) < 0)
    {
        perror("Can't connect to shared memory");
        exit(-1);
    }
    else
    {
        if (ftruncate(main_shmid, field_size * sizeof(int)) < 0)
        {
            perror("Can't rezie shm");
            exit(-1);
        }
        if ((field = mmap(0, field_size * sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED, main_shmid, 0)) < 0)
        {
            printf("Can\'t connect to shared memory\n");
            exit(-1);
        };
        printf("Open shared Memory for field\n");
    }

    if ((sem_main_shmid = shm_open(sem_shared_object, O_CREAT | O_RDWR, 0666)) < 0)
    {
        perror("Can't connect to shared memory");
        exit(-1);
    }
    else
    {
        if (ftruncate(sem_main_shmid, sem_count * sizeof(sem_t)) < 0)
        {
            perror("Can't rezie shm");
            exit(-1);
        }
        if ((semaphores = mmap(0, sem_count * sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED, sem_main_shmid, 0)) < 0)
        {
            printf("Can\'t connect to shared memory for semaphores\n");
            exit(-1);
        };
        printf("Open shared Memory for semaphores\n");
    }

    initializeField(field);
    printField(field);
    // Создаем семафоры для каждого из блоков плана
    createSemaphores(semaphores);
    fflush(stdout);
    chpid1 = fork();
    if (chpid1 == 0)
    {
        launchFirstGardener(columns, rows, first_gardener_working_time);
    }
    else if (chpid1 < 0)
    {
        perror("Can't run first gardener");
        exit(-1);
    }
    chpid2 = fork();
    if (chpid2 == 0)
    {
        launchSecondGardener(columns, rows, second_gardener_working_time);
    }
    else if (chpid2 < 0)
    {
        perror("Can't run second gardener");
        exit(-1);
    }
    signal(SIGINT, keyboard_interruption_handler);
    int status = 0;
    waitpid(chpid1, &status, 0);
    waitpid(chpid2, &status, 0);
    printField(field);
    fflush(stdout);
    shm_unlink(shared_object);
}
