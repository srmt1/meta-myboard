#include "sensor_i2c_mutex.h"

#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <stdint.h>

#define SHM_NAME "/sensor_i2c_mutex"
#define MAGIC    0x53494D58U

struct shared_mutex {
    uint32_t magic;
    pthread_mutex_t mutex;
};

int sensor_i2c_mutex_open(void)
{
    int fd;
    int created = 0;
    struct shared_mutex *shared;
    pthread_mutexattr_t attr;

    fd = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL, 0666);

    if (fd >= 0) {
        created = 1;

        if (ftruncate(fd, sizeof(struct shared_mutex)) == -1) {
            close(fd);
            shm_unlink(SHM_NAME);
            return -1;
        }
    } else {
        fd = shm_open(SHM_NAME, O_RDWR, 0666);

        if (fd == -1)
            return -1;
    }

    /*
     * Use an advisory file lock while checking/initialising
     * the shared memory object.
     */
    if (flock(fd, LOCK_EX) == -1) {
        close(fd);
        return -1;
    }

    shared = mmap(NULL,
                  sizeof(struct shared_mutex),
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED,
                  fd,
                  0);

    if (shared == MAP_FAILED) {
        flock(fd, LOCK_UN);
        close(fd);

        if (created)
            shm_unlink(SHM_NAME);

        return -1;
    }

    if (created || shared->magic != MAGIC) {
        if (pthread_mutexattr_init(&attr) != 0) {
            munmap(shared, sizeof(struct shared_mutex));
            flock(fd, LOCK_UN);
            close(fd);

            if (created)
                shm_unlink(SHM_NAME);

            return -1;
        }

        if (pthread_mutexattr_setpshared(
                &attr,
                PTHREAD_PROCESS_SHARED) != 0) {

            pthread_mutexattr_destroy(&attr);
            munmap(shared, sizeof(struct shared_mutex));
            flock(fd, LOCK_UN);
            close(fd);

            if (created)
                shm_unlink(SHM_NAME);

            return -1;
        }

        if (pthread_mutex_init(&shared->mutex, &attr) != 0) {
            pthread_mutexattr_destroy(&attr);
            munmap(shared, sizeof(struct shared_mutex));
            flock(fd, LOCK_UN);
            close(fd);

            if (created)
                shm_unlink(SHM_NAME);

            return -1;
        }

        pthread_mutexattr_destroy(&attr);

        /*
         * Write the magic value only after the mutex has
         * been successfully initialised.
         */
        shared->magic = MAGIC;
    }

    munmap(shared, sizeof(struct shared_mutex));

    flock(fd, LOCK_UN);

    return fd;
}

int sensor_i2c_mutex_lock(int fd)
{
    struct shared_mutex *shared;
    int result;

    shared = mmap(NULL,
                  sizeof(struct shared_mutex),
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED,
                  fd,
                  0);

    if (shared == MAP_FAILED)
        return -1;

    result = pthread_mutex_lock(&shared->mutex);

    munmap(shared, sizeof(struct shared_mutex));

    return result;
}

int sensor_i2c_mutex_unlock(int fd)
{
    struct shared_mutex *shared;
    int result;

    shared = mmap(NULL,
                  sizeof(struct shared_mutex),
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED,
                  fd,
                  0);

    if (shared == MAP_FAILED)
        return -1;

    result = pthread_mutex_unlock(&shared->mutex);

    munmap(shared, sizeof(struct shared_mutex));

    return result;
}

int sensor_i2c_mutex_close(int fd)
{
    return close(fd);
}
