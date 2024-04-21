#include "common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <time.h>

int pid1 = -1;
int pid2 = -1;
int sem_main_id;
int special_sem_id;
int main_shmid;
int exit_shmid;
int global_columns;
int global_rows;
int *exit_data;

void printField(int *field, int columns, int rows){
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < columns; ++j) {
            if (field[i * columns + j] < 0) {
                printf("X ");
            }
            else {
                printf("%d ", field[i * columns + j]);
            }
        }
        printf("\n");
    }
}

void initializeField(int *field, int columns, int rows) {
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < columns; ++j) {
            field[i * columns + j] = 0;
        }
    }

    int percentage = 10 + random() % 20;
    int count_of_bad_plots = columns * rows * percentage / 100;
    for (int i = 0; i < count_of_bad_plots; ++i) {
        int row_index;
        int column_index;
        do {
            row_index = random() % rows;
            column_index = random() % columns;
        } while (field[row_index * columns + column_index] == -1);
        field[row_index * columns + column_index] = -1;
    }
}

void initializeSemaphores(int sem_id, int sem_count) {
    for (int k = 0; k < sem_count; ++k) {
        semctl(sem_id, k, SETVAL, 1);
        if (semctl(sem_id, k, GETVAL) != 1) {
            printf("Can't set initial value of semaphore to 1\n");
            exit(-1);
        }
    }
}

void keyboard_handler(int num)
{
    printf("Closing resources...\n");
    exit_data[0] = 1;
    deleteSemaphores(sem_main_id);
    deleteSharedMemory(main_shmid);

    if (pid1 > 0 && exit_data[1] == 0) {
        kill(pid1, SIGINT);
    }
    if (pid2 > 0 && exit_data[2] == 0)
    {
        kill(pid2, SIGINT);
    }

    struct shmid_ds info;
    shmctl(exit_shmid, IPC_STAT, &info);
    if (info.shm_nattch == 1)
    {
        deleteSharedMemory(exit_shmid);
        deleteSemaphores(special_sem_id);
    }

    exit(0);
}

int main(int argc, char *argv[])
{
    srand(time(NULL));

    if (argc != 2) {
        printf("Invalid count of arguments. Expected 1: square_side_size\n");
        exit(-1);
    }

    int square_side_size = atoi(argv[1]);
    if (square_side_size * square_side_size > MAX_OF_SEMAPHORES) {
        printf("Too large square_side_size\n");
        exit(-1);
    }
    else if (square_side_size < 2) {
        printf("Too small square_side_size\n");
        exit(-1);
    }

    int rows = 2 * square_side_size;
    int columns = 2 * square_side_size;
    global_columns = global_rows = 2 * square_side_size;
    int field_size = rows * columns;
    int *field;
    int *meta_data;
    int *shm_data;

    exit_data = createOrOpenExitSharedMemory(&exit_shmid);
    signal(SIGINT, keyboard_handler);
    key_t sem_key;
    key_t shm_key;
    key_t special_sem_key;
    if ((shm_key = ftok(key_file_name, 2)) < 0) {
        printf("Can't generate shared memory key\n");
        exit(-1);
    }

    if ((sem_key = ftok(key_file_name, 3)) < 0) {
        printf("Can't generate sem key\n");
        exit(-1);
    }

    if ((special_sem_key = ftok(special_sem_key_file_name, 1)) < 0) {
        printf("Can't generate special sem key\n");
        exit(-1);
    }

    if ((main_shmid = shmget(shm_key, (field_size + 5) * sizeof(int), 0666 | IPC_CREAT | IPC_EXCL)) < 0) {
        if ((main_shmid = shmget(shm_key, (field_size + 5) * sizeof(int), 0666 | IPC_CREAT)) < 0) {
            perror("Can't connect to shared memory");

            exit(-1);
        }
    }

    if ((shm_data = shmat(main_shmid, 0, 0)) == NULL) {
        printf("Can\'t get shared memory\n");
        exit(-1);
    }
    printf("Open shared Memory for field\n");
    int sem_count = field_size / 4;
    if ((sem_main_id = semget(sem_key, sem_count, 0666 | IPC_CREAT)) < 0) {
        perror("Can't create semaphores");
        deleteSharedMemory(main_shmid);
        exit(-1);
    }
    special_sem_id = createOrOpenSemaphore(special_sem_key, 3, 0);
    field = shm_data + 5;
    meta_data = shm_data;
    meta_data[0] = columns;
    meta_data[1] = rows;
    meta_data[2] = getpid();
    initializeField(field, columns, rows);
    printField(field, columns, rows);
    initializeSemaphores(sem_main_id, sem_count);
    fflush(stdout);

    struct sembuf waiter_buf;
    waiter_buf.sem_num = 0;
    waiter_buf.sem_op = 2;
    waiter_buf.sem_flg = 0;

    if (semop(special_sem_id, &waiter_buf, 1) < 0) {
        keyboard_handler(1);
    }

    struct sembuf pid_waiter_buf;
    pid_waiter_buf.sem_num = 1;
    pid_waiter_buf.sem_flg = 0;
    pid_waiter_buf.sem_op = -2;
    if (semop(special_sem_id, &pid_waiter_buf, 1) < 0) {
        keyboard_handler(1);
    }

    pid1 = meta_data[3];
    pid2 = meta_data[4];
    int first_gardener_pid = meta_data[3];
    int second_gardener_pid = meta_data[4];
    struct sembuf finish_waiter_buf;
    finish_waiter_buf.sem_num = 2;
    finish_waiter_buf.sem_flg = 0;
    finish_waiter_buf.sem_op = -2;

    if (semop(special_sem_id, &finish_waiter_buf, 1) < 0) {
        keyboard_handler(1);
    }

    deleteSemaphores(sem_main_id);
    printf("\nResult:\n");
    printField(field, columns, rows);
    fflush(stdout);
    deleteSharedMemory(main_shmid);
    deleteSharedMemory(exit_shmid);
    deleteSemaphores(special_sem_id);
    printf("Deleting shared memory and semaphores...\n");

    return 0;
}
