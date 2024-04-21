#include "common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>

int *ex_data;
int ex_shmid;
int handler_pid;
int added_sem_id;

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
        printf("Invalid arguments. Expected 1: work_time\n");
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
    struct sembuf waiter_buf;
    waiter_buf.sem_num = 0;
    waiter_buf.sem_op = -1;
    waiter_buf.sem_flg = 0;
    printf("Gardener 1 waiting...\n");
    semop(added_sem_id, &waiter_buf, 1);
    printf("Gardener 1 after wait\n");
    signal(SIGINT, keyboard_handler);
    int *field;
    int *meta_data;
    int shmid;

    int cols;
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
            printf("Can't connect to shared memory\n");
            exit(-1);
        }
    }

    handler_pid = meta_data[2];
    cols = meta_data[0];
    rows = meta_data[1];
    int big_columns = cols / 2;
    int field_size = cols * rows;
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
    printf("Gardener 1 open shared memory with field\n");

    meta_data[3] = getpid();

    struct sembuf pid_waiter_buf;
    pid_waiter_buf.sem_num = 1;
    pid_waiter_buf.sem_flg = 0;
    pid_waiter_buf.sem_op = 1;

    semop(added_sem_id, &pid_waiter_buf, 1);

    int sem_id;
    getSemaphores(cols, rows, &sem_id);
    fflush(stdout);

    int i = 0;
    int j = 0;
    struct GardenerTask task;
    task.gardener_id = 1;
    task.work_time = workingTimeMilliseconds;
    while (i < rows) {
        while (j < cols) {
            task.p_i = i;
            task.p_j = j;
            handleGardenPlot(sem_id, field, big_columns, task);
            ++j;
        }
        ++i; --j;
        while (j >= 0) {
            task.p_i = i;
            task.p_j = j;
            handleGardenPlot(sem_id, field, big_columns, task);
            --j;
        }
        ++i; ++j;
    }
    printf("Gardener 1 finish work\n");

    struct sembuf waiter_final_buf;
    waiter_final_buf.sem_num = 2;
    waiter_final_buf.sem_flg = 0;
    waiter_final_buf.sem_op = 1;
    semop(added_sem_id, &waiter_final_buf, 1);
    return 0;
}
