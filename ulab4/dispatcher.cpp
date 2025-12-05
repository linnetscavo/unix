#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <openssl/sha.h>

struct Task {
    int id;
    char input[256];
    char hash[65];  
    volatile int status;  
};

int main() {
    const char* shm_name = "/lab4_shm";
    const char* sem_task_name = "/lab4_task_sem";
    const char* sem_done_name = "/lab4_done_sem";

    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        std::cerr << "dispatcher: shm_open failed\n";
        return 1;
    }
    ftruncate(shm_fd, sizeof(Task));
    Task* task = (Task*)mmap(NULL, sizeof(Task), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (task == MAP_FAILED) {
        std::cerr << "dispatcher: mmap failed\n";
        return 1;
    }

    sem_t* sem_task = sem_open(sem_task_name, O_CREAT | O_EXCL, 0666, 0);
    sem_t* sem_done = sem_open(sem_done_name, O_CREAT | O_EXCL, 0666, 0);
    if (sem_task == SEM_FAILED || sem_done == SEM_FAILED) {
        std::cerr << "dispatcher: sem_open failed\n";
        return 1;
    }

    std::cout << "Dispatcher готов. Введите строку (или 'quit'):\n";
    std::string input;
    int id = 1;

    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        if (input == "quit") break;

        task->id = id++;
        strncpy(task->input, input.c_str(), 255);
        task->input[255] = '\0';
        task->status = 0;

        sem_post(sem_task);

        sem_wait(sem_done);

        std::cout << "→ Хеш: " << task->hash << "\n";
    }

    munmap(task, sizeof(Task));
    shm_unlink(shm_name);
    sem_close(sem_task);
    sem_close(sem_done);
    sem_unlink(sem_task_name);
    sem_unlink(sem_done_name);
    return 0;
}