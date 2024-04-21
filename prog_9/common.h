#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <time.h>

struct GardenerTask
{
    int p_i;
    int p_j;
    int gardener_id;
    int work_time;
};

const char *main_waiter_sem = "/sem-main-waiter";
const char *pid_waiter_sem = "/sem-pid-waiter";
const char *finish_sem = "/finish-sem";
const int MAX_OF_SEMAPHORES = 1024;
const char *shared_object = "/posix-shared-object";
const char *exit_key = "/exit-shared-object";
const char *exit_shared_object = "exit_key";
char *sempahore_template_name = "/garden-semaphore-id-";
const char *key_file_name = "system_V_key2";
const char *special_sem_key_file_name = "special_semaphores_key";
const int EMPTY_PLOT_COEFFICIENT = 2;

void deleteSemaphores(int sem_id) {

    if (semctl(sem_id, 0, IPC_RMID) < 0) {
        printf("Can't delete semaphores\n");
        exit(-1);
    }
}

void deleteSharedMemory(int shm_id) {
    if (shmctl(shm_id, IPC_RMID, NULL) < 0) {
        printf("Can't delete shm\n");
        exit(-1);
    }
}

int createOrOpenSemaphore(key_t key, int count, int value)
{
    int sem;
    if ((sem = semget(key, count, 0666 | IPC_CREAT | IPC_EXCL)) < 0) {
        if ((sem = semget(key, count, 0666 | IPC_CREAT)) < 0) {
            perror("Can't open semaphores");
            exit(-1);
        }
    }
    else {
        semctl(sem, 0, SETVAL, value);
    }
    return sem;
}

int *createOrOpenExitSharedMemory(int *exit_shmid) {
    key_t shm_key;
    if ((shm_key = ftok(exit_shared_object, 0)) < 0) {
        perror("Can't generate shared memory key");
        exit(-1);
    }
    int *exit_data;
    if ((*exit_shmid = shmget(shm_key, 3 * sizeof(int), 0666 | IPC_CREAT | IPC_EXCL)) < 0) {
        if ((*exit_shmid = shmget(shm_key, 3 * sizeof(int), 0666)) < 0) {
            perror("Can't connect to shared memory");
            exit(-1);
        }
        if ((exit_data = shmat(*exit_shmid, 0, 0)) == NULL) {
            printf("Can\'t get shared memory\n");
            exit(-1);
        }
    }
    else {
        if ((exit_data = shmat(*exit_shmid, 0, 0)) == NULL) {
            printf("Can\'t get shared memory\n");
            exit(-1);
        }
        exit_data[0] = 0;
        exit_data[1] = 0;
        exit_data[2] = 0;
    }

    return exit_data;
}

void getSemaphores(int columns, int rows, int *sem_id) {
    key_t sem_key;
    if ((sem_key = ftok(key_file_name, 3)) < 0) {
        printf("Can't generate sem key\n");
        exit(-1);
    }
    if ((*sem_id = semget(sem_key, columns * rows / 4, 0)) < 0) {
        printf("Can't open semaphores\n");
        exit(-1);
    }
}

void handleGardenPlot(int sem_id, int *field, int big_columns, struct GardenerTask task) {
    struct sembuf buf;
    buf.sem_num = task.p_i / 2 * big_columns + task.p_j / 2;

    buf.sem_op = -1;
    buf.sem_flg = 0;
    semop(sem_id, &buf, 1);
    printf("Gardener %d on row: %d, col: %d\n", task.gardener_id, task.p_i, task.p_j);
    fflush(stdout);
    if (field[task.p_i * 2 * big_columns + task.p_j] == 0) {
        field[task.p_i * 2 * big_columns + task.p_j] = task.gardener_id;
        usleep(task.work_time * 1000);
    }
    else {
        usleep(task.work_time / EMPTY_PLOT_COEFFICIENT * 1000);
    }
    buf.sem_op = 1;
    buf.sem_flg = 0;
    semop(sem_id, &buf, 1);
}
