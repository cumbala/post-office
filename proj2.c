///
/// @file proj2.c
/// @brief Implementation of 2nd project for IOS subject
/// @date 30.04.2023
/// @author Konstantin Romanets (xroman18), xroman18(at)stud.fit.vutbr.cz
///
/// @copyright VUT FIT 2023
///

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <wait.h>
#include <errno.h>
#include <sys/mman.h>

/// Sets seed for rand() \n
/// time(NULL) xor getpid() is random enough
#define SET_RANDOM srand(time(NULL) ^ getpid()); // NOLINT(cert-msc51-cpp)

/// Shortcut to call rand() with min and max values
#define RAND(min, max) (rand() % (max - min + 1) + min) // NOLINT(cert-msc50-cpp

/// Calls sem_init and checks for error during semaphore creation
#define INIT_SEM(sem, pshared, value) if (sem_init(&sem, pshared, value) == -1) error("Failed to initialize semaphore")

/// Calls sem_destroy and checks for error during semaphore destruction
#define DEST_SEM(sem) if (sem_destroy(&sem) == -1) error("Failed to destroy semaphore")

/// Debug messages.
/// Compile with -DDEBUG to see them.
#ifdef DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

/// Represents worker's actions.
enum WorkerAction {
    // Started
    WA_STARTED,
    // Serving a service
    WA_SERVING_START,
    // Service finished
    WA_SERVING_END,
    // Taking a break
    WA_BREAK_START,
    // Break finished
    WA_BREAK_END,
    // Going home
    WA_FINISHED
};

/// Represents client's actions.
enum ClientAction {
    // Started
    CA_STARTED,
    // Entering office for service
    CA_ENTERING_OFFICE,
    // Called by office worker
    CA_CALLED_BY_WORKER,
    // Going home
    CA_FINISHED
};

/// Program's shared memory \n
/// Contains all semaphores and other shared variables
typedef struct mem {
    // Output file lines count
    size_t lines_count;

    // Is post office open
    bool post_open;
    // Queues of clients
    int queue[3];

    // Output file mutex
    sem_t output;
    // Global mutex guard
    sem_t mutex;
    // Queues semaphores that indicate availability
    sem_t queue_sem[3];
    // Post is closed notification
    sem_t post_closed;
    // Worker is leaving
    sem_t leaving;

    // Output file
    FILE* file;
} SharedMemory;

/// @brief Prints error message and exits with EXIT_FAILURE
/// @param msg Error message
void error(const char* msg) {
    fprintf(stderr, "[ERROR] %s\n", msg);
    exit(EXIT_FAILURE);
}

/// @brief Checks if number is in range
/// @param num Number to check
/// @param min Minimum value
/// @param max Maximum value
/// @return 1 if number is in range, 0 otherwise
bool check_range(int num, int min, int max) {
    return num >= min && num <= max;
}

/// @brief Parses integer argument
/// @param arg Argument to parse
/// @return Parsed integer or -1 if error occurred
/// @note Exits with EXIT_FAILURE if error occurred
int parse_int_arg(char* arg) {
    char* temp;
    long result = strtol(arg, &temp, 10);
    if (strlen(temp) != 0) {
        error("Invalid argument");
        return -1;
    } else {
        return (int)result;
    }
}

/// @brief Initiates process sleep for random time between min and max
/// @param min Minimum sleep time in seconds
/// @param max Maximum sleep time in seconds
void random_sleep(int min, int max) {
    SET_RANDOM
    int time = RAND(min, max); // NOLINT(cert-msc50-cpp)
    usleep(time * 1000);
}

/// @brief Generates random integer between min and max
/// @param min Minimum value
/// @param max Maximum value
int random_int(int min, int max) {
    SET_RANDOM
    return RAND(min, max); // NOLINT(cert-msc50-cpp)
}

/// @brief Initializes all semaphores in SharedMemory
/// @param memory SharedMemory to initialize
void Semaphores_init(SharedMemory *memory) {
    INIT_SEM(memory->mutex, 1, 1);
    INIT_SEM(memory->output, 1, 1);
    INIT_SEM(memory->leaving, 1, 0);
    INIT_SEM(memory->post_closed, 1, 0);

    for (int i = 0; i < 3; i++)
        INIT_SEM(memory->queue_sem[i], 1, 0);
}

/// @brief Destroys all semaphores in SharedMemory
/// @param memory SharedMemory to destroy
void Semaphores_destroy(SharedMemory *memory) {
    DEST_SEM(memory->mutex);
    DEST_SEM(memory->output);
    DEST_SEM(memory->leaving);
    DEST_SEM(memory->post_closed);

    for (int i = 0; i < 3; i++)
        DEST_SEM(memory->queue_sem[i]);
}

/// @brief Initializes SharedMemory
/// @return Pointer to initialized SharedMemory
/// @note Exits with EXIT_FAILURE if error occurred
SharedMemory* SharedMemory_init(void) {
    SharedMemory *memory = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED)
        error("Failed to allocate memory");

    memory->lines_count = 0;
    memory->post_open = 1;

    memory->file = fopen("proj2.out", "w");
    if (memory->file == NULL)
        error("Failed to open output file");

    Semaphores_init(memory);
    return memory;
}

/// @brief Destroys SharedMemory
/// @param memory SharedMemory to destroy
void SharedMemory_destroy(SharedMemory *memory) {
    Semaphores_destroy(memory);
    fclose(memory->file);
    if (munmap(memory, sizeof(SharedMemory)) == -1)
        error("Failed to free memory");
}

/// @brief Logs client's action
/// @param memory SharedMemory to get output file
/// @param id Client's number (1..NU)
/// @param service Type of service (1..3), 0 if no service
/// @param action Client's action
void log_client(SharedMemory* memory, u_int id, u_int service, enum ClientAction action) {
    sem_wait(&memory->output);

    switch(action) {
        case CA_STARTED:
            fprintf(memory->file, "%ld: Z %d: started\n", ++memory->lines_count, id);
            break;
        case CA_ENTERING_OFFICE:
            fprintf(memory->file, "%ld: Z %d: entering office for a service %d\n", ++memory->lines_count, id, service);
            break;
        case CA_CALLED_BY_WORKER:
            fprintf(memory->file, "%ld: Z %d: called by office worker\n", ++memory->lines_count, id);
            break;
        case CA_FINISHED:
            fprintf(memory->file, "%ld: Z %d: going home\n", ++memory->lines_count, id);
            break;
    }

    fflush(memory->file);

    sem_post(&memory->output);
}

/// @brief Logs worker's action
/// @param memory SharedMemory to get output file
/// @param id Worker's number (1..NZ)
/// @param service Type of service (1..3), 0 if no service
/// @param action Worker's action
void log_worker(SharedMemory* memory, u_int id, u_int service, enum WorkerAction action) {
    sem_wait(&memory->output);

    switch(action) {
        case WA_STARTED:
            fprintf(memory->file, "%ld: U %d: started\n", ++memory->lines_count, id);
            break;
        case WA_SERVING_START:
            fprintf(memory->file, "%ld: U %d: serving a service of type %d\n", ++memory->lines_count, id, service);
            break;
        case WA_SERVING_END:
            fprintf(memory->file, "%ld: U %d: service finished\n", ++memory->lines_count, id);
            break;
        case WA_BREAK_START:
            fprintf(memory->file, "%ld: U %d: taking break\n", ++memory->lines_count, id);
            break;
        case WA_BREAK_END:
            fprintf(memory->file, "%ld: U %d: break finished\n", ++memory->lines_count, id);
            break;
        case WA_FINISHED:
            fprintf(memory->file, "%ld: U %d: going home\n", ++memory->lines_count, id);
            break;
    }

    fflush(memory->file);

    sem_post(&memory->output);
}

/// @brief Logs post office's action
/// @param memory SharedMemory to get output file
/// @note Post has only one function - closing
void log_office(SharedMemory* memory) {
    sem_wait(&memory->output);

    fprintf(memory->file, "%ld: closing\n", ++memory->lines_count);
    fflush(memory->file);

    sem_post(&memory->output);
}

/// @brief Client's process
/// @param memory Program's shared memory
/// @param id Client's number (1..NU)
/// @param TZ Maximum time to sleep before entering the office
void process_client(SharedMemory *memory, u_int id, int TZ) {
    log_client(memory, id, 0, CA_STARTED);
    PRINT("[C] Client %d started\n", id);

    // Sleep before entering the office
    random_sleep(0, TZ);

    // Select a service
    int service = random_int(1, 3);

    // Check synchronously if post office is open
    // Otherwise, we can encounter a situation
    // when client is trying to enter the post after
    // it's closed
    bool is_post_open;
    sem_wait(&memory->mutex);
    is_post_open = memory->post_open;
    sem_post(&memory->mutex);

    if (is_post_open && (is_post_open == memory->post_open)) {
        PRINT("[C] Post: %d; Client %d; Service: %d; Entering office\n", memory->post_open, id, service);
        // Increment queue for selected service
        sem_wait(&memory->mutex);
        memory->queue[service - 1]++;
        sem_post(&memory->mutex);

        log_client(memory, id, service, CA_ENTERING_OFFICE);
    } else {
        PRINT("[C] Post: %d; Client %d; Service: %d; Finished\n", memory->post_open, id, service);
        log_client(memory, id, service, CA_FINISHED);
        exit(EXIT_SUCCESS);
    }

    // Wait for worker to call client
    sem_wait(&memory->queue_sem[service - 1]);

    // Get serviced for random time
    PRINT("[C] Post: %d; Client %d; Service: %d; Called by worker\n", memory->post_open, id, service);
    log_client(memory, id, service, CA_CALLED_BY_WORKER);
    random_sleep(0, 10);
    log_client(memory, id, service, CA_FINISHED);

    PRINT("[C] Post: %d; Client %d; Service: %d; Finished\n", memory->post_open, id, service);
    exit(EXIT_SUCCESS);
}

/// @brief Checks if there are any clients waiting in queues
/// @param memory Program's shared memory
bool check_queues(SharedMemory *memory) {
    return memory->queue[0] > 0 || memory->queue[1] > 0 || memory->queue[2] > 0;
}

/// @brief Worker's process
/// @param memory Program's shared memory
/// @param id Worker's number (1..NU)
/// @param TU Maximum time to sleep before starting work
void process_worker(SharedMemory *memory, u_int id, int TU) {
    log_worker(memory, id, 0, WA_STARTED);
    PRINT("[W] Worker %d started\n", id);

    bool has_clients;    // if there are any customers waiting = 1, else = 0
    bool post_open;           // if office is open = 1, else = 0
    bool is_leaving = false;      // if office semaphore is posted = 1, else = 0

    while (true) {
        sem_wait(&memory->mutex);
        has_clients = check_queues(memory);
        post_open = memory->post_open;
        sem_post(&memory->mutex);

        PRINT("[W] Post: %d; Worker %d; Customers waiting: [%d, %d, %d];\n", post_open, id, memory->queue[0], memory->queue[1], memory->queue[2]);

        // If any of the queues are not empty
        if (has_clients) {
            PRINT("[W] Post: %d; Worker %d; Customers waiting: %d; Serving started\n", post_open, id, has_clients);

            // Choose a queue
            // Randomly at first, then just first not empty queue
            int queue = random_int(1, 3);
            if (memory->queue[queue - 1] == 0)
                for (queue = 1; queue <= 3; queue++)
                    if (memory->queue[queue - 1]) break;

            PRINT("[W] Post: %d; Worker %d; Chosen queue: %d\n", post_open, id, queue);

            sem_wait(&memory->mutex);
            memory->queue[queue - 1]--;
            sem_post(&memory->mutex);

            // Let customer in the queue enter the office
            sem_post(&memory->queue_sem[queue - 1]);

            // Start serving
            PRINT("[W] Post: %d; Worker %d; Service: %d; Serving\n", post_open, id, queue);
            log_worker(memory, id, queue, WA_SERVING_START);
            random_sleep(0, 10);
            log_worker(memory, id, queue, WA_SERVING_END);
            PRINT("[W] Post: %d; Worker %d; Service: %d; Serving done\n", post_open, id, queue);
            continue;
        }
        // Empty queue, but post is still open
        if (!has_clients && post_open) {
            // Ensure that post is still open
            if (!memory->post_open) {
                PRINT("[W] Post: %d; Worker %d; Service: 0; Post closed!\n", post_open, id);
                continue;
            }

            PRINT("[W] Post: %d; Worker %d; Service: 0; Taking break\n", post_open, id);
            log_worker(memory, id, 0, WA_BREAK_START);

            // Office may be closed during break, so we need to tell worker that it's time to leave
            if (!memory->post_open) {
                PRINT("[W] Post: %d; Worker %d; Service: 0; Post closed, sem posted!\n", post_open, id);
                sem_post(&memory->leaving);
                is_leaving = 1;
            }
            random_sleep(0, TU);
            log_worker(memory, id, 0, WA_BREAK_END);
            PRINT("[W] Post: %d; Worker %d; Service: 0; Break done\n", post_open, id);
            continue;
        }
        // Empty queue and post is closed - can safely finish
        if (!has_clients && !post_open) {
            PRINT("[W] Post: %d; Worker %d; Service: 0; Post closed, preparing to leave\n", post_open, id);
            // Ensure that there are no customers waiting
            if (check_queues(memory)) {
                PRINT("[W] Post: %d; Worker %d; Service: 0; Post closed, customers waiting\n", post_open, id);
                continue;
            }

            // Post leaving semaphore
            if (!is_leaving) {
                PRINT("[W] Post: %d; Worker %d; Service: 0; Post closed, no clients left, sem posted!\n", post_open, id);
                sem_post(&memory->leaving);
            }

            // Unblock any remaining customers
            for (int i = 0; i < 3; i++) {
                if (memory->queue[i] > 0) {
                    sem_post(&memory->queue_sem[i]);
                }
            }

            PRINT("[W] Post: %d; Worker %d; Service: 0; Post closed, waiting for main process message\n", post_open, id);
            // Wait for main process to allow worker to finish
            sem_wait(&memory->post_closed);

            // Finally, finish
            PRINT("[W] Post: %d; Worker %d; Service: 0; Finished\n", post_open, id);
            log_worker(memory, id, 0, WA_FINISHED);
            exit(EXIT_SUCCESS);
        }

        // Something seriously went wrong, if we dropped here
        // It did happen before, but I wasn't able to reproduce it
        PRINT("[W] Post: %d; Worker %d; Service: 0; We shouldn't be here...\n", post_open, id);
    }
}

/// @brief Main function
/// @param argc Number of arguments
/// @param argv Arguments' array
int main(int argc, char** argv) {
    // Check arguments count
    if (argc != 6)
        error("Invalid number of arguments");

    // Parse all arguments
    int NZ = parse_int_arg(argv[1]);
    int NU = parse_int_arg(argv[2]);
    int TZ = parse_int_arg(argv[3]);
    int TU = parse_int_arg(argv[4]);
    int F  = parse_int_arg(argv[5]);

    // Check arguments' ranges
    if (!(  NZ > 0
        &&  NU > 0
        &&  check_range(TZ, 0, 10000)
        &&  check_range(TU, 0, 100)
        &&  check_range(F, 0, 10000)
    ))
        error("Invalid input arguments");

    PRINT("[M] NZ: %d, NU: %d, TZ: %d, TU: %d, F: %d\n", NZ, NU, TZ, TU, F);

    // Initialize shared memory
    SharedMemory *shared = SharedMemory_init();

    // Fork clients
    for (int i = 0; i < NZ; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            process_client(shared, i + 1, TZ);
        } else if (pid < 0) {
            SharedMemory_destroy(shared);
            error("Failed to fork a process");
        }
    }

    // Fork workers
    for (int i = 0; i < NU; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            process_worker(shared, i + 1, TU);
        } else if (pid < 0) {
            SharedMemory_destroy(shared);
            error("Failed to fork a process");
        }
    }

    PRINT("[M] Randomly sleeping\n");

    // Do that random sleep
    random_sleep(F / 2, F);

    PRINT("[M] Done sleeping, closing post\n");

    // Close the office
    shared->post_open = 0;

    PRINT("[M] Post is closed, waiting for workers to finish\n");
    // Wait for workers to finish
    for (int i = 0; i < NU; i++) {
        sem_wait(&shared->leaving);
    }
    // Print closing message
    log_office(shared);

    PRINT("[M] Sending message to workers\n");
    // Tell workers that they can finish
    for (int i = 0; i < NU; i++) {
        sem_post(&shared->post_closed);
    }

    PRINT("[M] Waiting for children processes\n");
    // Wait for all children to finish
    while (wait(NULL))
        if (errno == ECHILD) break;

    // Clean up
    SharedMemory_destroy(shared);

    PRINT("[M] Done\n");

    return 0;
}
