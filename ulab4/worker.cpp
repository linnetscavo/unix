#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <openssl/sha.h>

struct Task {
    int id;
    char input[256];
    char hash[65];
    volatile int status;
};

std::string sha1_str(const char* s) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char*)s, strlen(s), hash);
    char hex[SHA_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        snprintf(&hex[i*2], 3, "%02x", hash[i]);
    }
    return std::string(hex, SHA_DIGEST_LENGTH * 2);
}

int main() {
    const char* shm_name = "/lab4_shm";
    const char* sem_task_name = "/lab4_task_sem";
    const char* sem_done_name = "/lab4_done_sem";

    int shm_fd = shm_open(shm_name, O_RDWR, 0);
    if (shm_fd == -1) {
        std::cerr << "worker: shm_open failed\n";
        return 1;
    }
    Task* task = (Task*)mmap(NULL, sizeof(Task), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (task == MAP_FAILED) {
        std::cerr << "worker: mmap failed\n";
        return 1;
    }

    sem_t* sem_task = sem_open(sem_task_name, 0);
    sem_t* sem_done = sem_open(sem_done_name, 0);
    if (sem_task == SEM_FAILED || sem_done == SEM_FAILED) {
        std::cerr << "worker: sem_open failed\n";
        return 1;
    }

    std::cout << "[Worker " << getpid() << "] готов\n";

    while (true) {
        sem_wait(sem_task);

        std::string hash = sha1_str(task->input);
        strncpy(task->hash, hash.c_str(), 64);
        task->hash[64] = '\0';
        task->status = 2;

        sem_post(sem_done);
    }

    munmap(task, sizeof(Task));
    sem_close(sem_task);
    sem_close(sem_done);
    return 0;
}