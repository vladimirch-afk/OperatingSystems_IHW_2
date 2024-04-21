#include "common.h"
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

int *ex_data;
int ex_shmid;
int added_sem_id;
int handler_pid;
void keyboard_handler(int num) {
    if (ex_data[0] != 1) {
        kill(handler_pid, SIGINT);
    }

    struct shmid_ds info;
    shmctl(ex_shmid, IPC_STAT, &info);
    if (info.shm_nattch == 1) {
        deleteSharedMemory(ex_shmid);
        deleteSemaphores(added_sem_id);
    }

    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Invalid arguments. Expected 1: work_time_milliseconds\n");
        exit(-1);
    }

    ex_data = createOrOpenExitSharedMemory(&ex_shmid);
    int workingTimeMilliseconds = atoi(argv[1]);
    int special_sem_key;
    if ((special_sem_key = ftok(special_sem_key_file_name, 1)) < 0) {
        printf("Can't generate special sem key\n");
        exit(-1);
    }

    added_sem_id = createOrOpenSemaphore(special_sem_key, 3, 0);
    struct sembuf s_waiter_buf;
    s_waiter_buf.sem_num = 0;
    s_waiter_buf.sem_op = -1;
    s_waiter_buf.sem_flg = 0;

    printf("Gardener 2 waiting...\n");
    semop(added_sem_id, &s_waiter_buf, 1);
    printf("Gardener 2 after wait\n");
    signal(SIGINT, keyboard_handler);
    int *field;
    int *meta_data;
    int shmid;
    int columns;
    int rows;

    key_t shm_key;
    if ((shm_key = ftok(key_file_name, 2)) < 0) {
        printf("Can't generate shared memory key\n");
        exit(-1);
    }

    if ((shmid = shmget(shm_key, 5 * sizeof(int), 0666 | IPC_CREAT)) < 0) {
        perror("Can't connect to shared memory");
        exit(-1);
    }
    else {
        if ((meta_data = shmat(shmid, 0, 0)) == NULL) {
            printf("Can\'t connect to shared memory\n");
            exit(-1);
        }
    }

    handler_pid = meta_data[2];
    columns = meta_data[0];
    rows = meta_data[1];
    int big_columns = columns / 2;
    int field_size = columns * rows;

    if ((shmid = shmget(shm_key, (field_size + 5) * sizeof(int), 0666 | IPC_CREAT)) < 0) {
        perror("Can't connect to shared memory");
        exit(-1);
    }
    else {
        if ((field = shmat(shmid, 0, 0)) == NULL) {
            printf("Can\'t connect to shared memory\n");
            exit(-1);
        }
    }

    field = field + 5;
    meta_data[4] = getpid();

    struct sembuf waiter_buf_p;
    waiter_buf_p.sem_num = 1;
    waiter_buf_p.sem_flg = 0;
    waiter_buf_p.sem_op = 1;
    semop(added_sem_id, &waiter_buf_p, 1);
    int sem_id;
    getSemaphores(columns, rows, &sem_id);
    fflush(stdout);

    int i = rows - 1;
    int j = columns - 1;
    struct GardenerTask task;
    task.gardener_id = 2;
    task.work_time = workingTimeMilliseconds;
    while (j >= 0) {
        while (i >= 0) {
            task.p_i = i;
            task.p_j = j;
            handleGardenPlot(sem_id, field, big_columns, task);
            --i;
        }
        --j;++i;

        while (i < rows) {
            task.p_i = i;
            task.p_j = j;
            handleGardenPlot(sem_id, field, big_columns, task);
            ++i;
        }
        --i; --j;
    }
    printf("Gardener 2 finish work\n");

    struct sembuf finish_waiter_buf;
    finish_waiter_buf.sem_num = 2;
    finish_waiter_buf.sem_flg = 0;
    finish_waiter_buf.sem_op = 1;
    semop(added_sem_id, &finish_waiter_buf, 1);
    return 0;
}
