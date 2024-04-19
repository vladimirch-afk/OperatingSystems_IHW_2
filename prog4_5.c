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

const int MAX_OF_SEMAPHORES = 1024;
const int EMPTY_PLOT_COEFFICIENT = 2;
pid_t chpid1, chpid2;
char *sempahore_name = "/garden-semaphore";
const char *shared_object = "/posix-shared-object";
int m_shmid;
int columns;
int rows;
// Структура для определения задания садовника
struct GTask
{
    int p_i;
    int p_j;
    int gardener_id;
    int work_time;
};

// Вывести состояние сада в консоль
void plotGarden(sem_t **semaphores, int *field, int big_columns, struct GTask task)
{
    sem_wait(semaphores[task.p_i / 2 * big_columns + task.p_j / 2]);
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
    sem_post(semaphores[task.p_i / 2 * big_columns + task.p_j / 2]);
}

void acquireSemaphores(sem_t **semaphores, int columns, int rows)
{
    for (int k = 0; k < columns * rows / 4; ++k)
    {
        char sem_name[200];
        sprintf(sem_name, "%s%d", sempahore_name, k);

        if ((semaphores[k] = sem_open(sem_name, 0)) == 0)
        {
            perror("sem_open: Can not open semaphore");
            exit(-1);
        };
    }
}

// Получить поле
void getField(int **field, int field_size, int *shmid)
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
            printf("Can\'t connect to shared memory\n");
            exit(-1);
        };
    }
}

// Запустить первого садовника
void startFirstGardener(int columns, int rows, int workingTimeMilliseconds)
{
    int big_columns = columns / 2;
    int field_size = columns * rows;
    int *field;
    int shmid;
    sem_t *semaphores[MAX_OF_SEMAPHORES];
    acquireSemaphores(semaphores, columns, rows);
    getField(&field, field_size, &shmid);
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
            plotGarden(semaphores, field, big_columns, task);
            ++j;
        }
        ++i; --j;
        while (j >= 0)
        {
            task.p_i = i;
            task.p_j = j;
            plotGarden(semaphores, field, big_columns, task);
            --j;
        }
        ++i; ++j;
    }
    printf("Gardener 1 finished working...\n");
    exit(0);
}

// Выполнить второго садовника
void startSecondGardener(int columns, int rows, int workingTimeMilliseconds)
{
    int big_columns = columns / 2;
    int field_size = columns * rows;
    int *field;
    int shmid;
    sem_t *semaphores[MAX_OF_SEMAPHORES];
    acquireSemaphores(semaphores, columns, rows);
    getField(&field, field_size, &shmid);
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
            plotGarden(semaphores, field, big_columns, task);
            --i;
        }
        --j; ++i;
        while (i < rows)
        {
            task.p_i = i;
            task.p_j = j;
            plotGarden(semaphores, field, big_columns, task);
            ++i;
        }
        --i; --j;
    }
    printf("Gardener 2 finished working...\n");
    exit(0);
}

void unlink_all_semaphores(int columns, int rows)
{
    for (int k = 0; k < columns * rows / 4; ++k)
    {
        char sem_name[200];
        sprintf(sem_name, "%s%d", sempahore_name, k);

        sem_close(sem_open(sem_name, 0));
        if (sem_unlink(sem_name) < 0)
        {
            perror("Can't unlink semaphore");
            exit(-1);
        };
    }
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

// Создать семафоры под каждую клетку
void createSemaphores()
{
    for (int k = 0; k < columns * rows / 4; ++k)
    {
        char sem_name[200];
        sprintf(sem_name, "%s%d", sempahore_name, k);

        sem_t *sem;
        if ((sem = sem_open(sem_name, O_CREAT, 0666, 1)) == 0)
        {
            perror("Can not create semaphore");
            exit(-1);
        };

        int val;
        sem_getvalue(sem, &val);
        if (val != 1)
        {
            printf("Сan't set initial value to 1... \n");
            unlink_all_semaphores(columns, rows);
            shm_unlink(shared_object);
            exit(-1);
        }
    }
}

// Обрабатываем прерывание клавиатуры
void keyboard_handler(int num)
{
    kill(chpid1, SIGINT);
    kill(chpid2, SIGINT);
    printf("Closing resources...\n");
    shm_unlink(shared_object);
    unlink_all_semaphores(columns, rows);
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

// Создание поля
    rows = columns = 2 * square_side_size;
    int field_size = rows * columns;
    int first_gardener_working_time = atoi(argv[2]);
    int second_gardener_working_time = atoi(argv[3]);

    if (first_gardener_working_time < 1 || second_gardener_working_time < 1)
    {
        printf("Work time has to be bigger than 0\n");
        exit(-1);
    }

    int *field;

    if ((m_shmid = shm_open(shared_object, O_CREAT | O_RDWR, 0666)) < 0)
    {
        perror("Can't connect to shared memory");
        exit(-1);
    }
    else
    {
        if (ftruncate(m_shmid, field_size * sizeof(int)) < 0)
        {
            perror("Can't resize shm");
            exit(-1);
        }
        if ((field = mmap(0, field_size * sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED, m_shmid, 0)) < 0)
        {
            printf("Can't connect to shared memory\n");
            exit(-1);
        };
        printf("Open shared memory\n");
    }

// Создать сад
    initializeField(field);
    printField(field);

    // Создаем семафоры для каждого из блоков
    createSemaphores();
    fflush(stdout);

// Создать второй процесс
    chpid1 = fork();
    // Первый процесс - первый садовник
    if (chpid1 == 0)
    {
        startFirstGardener(columns, rows, first_gardener_working_time);
    }
    else if (chpid1 < 0)
    {
        perror("Can't run first gardener");
        exit(-1);
    }

// Второй процесс - второй садовник
    chpid2 = fork();
    if (chpid2 == 0)
    {
        startSecondGardener(columns, rows, second_gardener_working_time);
    }
    else if (chpid2 < 0)
    {
        perror("Can't run second gardener");
        exit(-1);
    }

    signal(SIGINT, keyboard_handler);
    int status = 0;
    waitpid(chpid1, &status, 0);
    waitpid(chpid2, &status, 0);
    unlink_all_semaphores(columns, rows);
    printField(field);
    fflush(stdout);
    shm_unlink(shared_object);
}
