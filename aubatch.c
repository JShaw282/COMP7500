#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

// Define a structure to represent a job
struct Job {
    char *name;
    int cpu_time;
    int priority;
    struct timeval arrival_time;
    int progress; // 0: Not started, 1: Run
    int is_running;
    struct Job *next; // Pointer to the next job in the linked list
};

// Synchronization objects
pthread_mutex_t job_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t job_available_cond = PTHREAD_COND_INITIALIZER;

// Shared flag to signal threads to exit
int exit_flag = 0;

// Head of the linked list representing the job queue
struct Job *job_queue_head = NULL;

// Function prototypes
void *scheduling_module(void *arg);
void *dispatching_module(void *arg);
void list_jobs();
void display_help();  // Added function for help

int main() {
    pthread_t scheduling_thread, dispatching_thread;

    // Initialize job_queue_mutex
    pthread_mutex_init(&job_queue_mutex, NULL);

    // Welcome message and prompt
    printf("Welcome to Jackson's batch job scheduler Version 1.0\n");
    printf("Type 'help' to find more about AUbatch commands.\n");
    printf("> ");

    // Command execution loop
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        // Parse the command
        char *command = strtok(buffer, " \n");

        if (strcmp(command, "help") == 0) {
            // Display help information
            display_help();
        } else if (strcmp(command, "run") == 0) {
            // Extract arguments for the "run" command
            char *job_name = strtok(NULL, " ");
            int execution_time = atoi(strtok(NULL, " "));
            int priority = atoi(strtok(NULL, " "));

            // Create a new job and add it to the job queue
            struct Job *new_job = malloc(sizeof(struct Job));
            new_job->name = strdup(job_name);
            new_job->cpu_time = execution_time;
            new_job->priority = priority;
            gettimeofday(&new_job->arrival_time, NULL);
            new_job->progress = 0;
            new_job->next = NULL;

            new_job->is_running = 0; // Initially not running

            // If this is the only job in the queue, consider it as running
            if (job_queue_head == NULL) {
            new_job->is_running = 1;
            }

            pthread_mutex_lock(&job_queue_mutex);

            // Insert the new job at the end of the linked list
            if (job_queue_head == NULL) {
                job_queue_head = new_job;
            } else {
                struct Job *current = job_queue_head;
                while (current->next != NULL) {
                    current = current->next;
                }
                current->next = new_job;
            }

            pthread_cond_signal(&job_available_cond);
            pthread_mutex_unlock(&job_queue_mutex);

            // Provide feedback for job submission
            printf("Job %s was submitted.\n", job_name);
            printf("Total number of jobs in the queue: ");
            printf("%d\n", get_num_jobs());
            printf("Scheduling Policy: FCFS.\n");
        } else if (strcmp(command, "list") == 0) {
            // Display queue information
            list_jobs();
        } else if (strcmp(command, "fcfs") == 0) {
            // ... (your existing code for the fcfs command)
        } else if (strcmp(command, "sjf") == 0) {
            // ... (your existing code for the sjf command)
        } else if (strcmp(command, "priority") == 0) {
            // ... (your existing code for the priority command)
        } else if (strcmp(command, "test") == 0) {
            // ... (your existing code for the test command)
        } else if (strcmp(command, "quit") == 0) {
            // Set the exit flag and signal threads to wake up
            exit_flag = 1;
            pthread_cond_signal(&job_available_cond);
            pthread_join(scheduling_thread, NULL);
            pthread_join(dispatching_thread, NULL);

            // Free the memory allocated for the linked list
            struct Job *current = job_queue_head;
            struct Job *next;
            while (current != NULL) {
                next = current->next;
                free(current->name);
                free(current);
                current = next;
            }

            break;
        } else {
            // Handle unknown command or errors
            printf("Unknown command. Type 'help' for more information.\n");
        }

        printf("> ");  // Prompt for the next command
    }

    printf("Main thread exiting...\n");
    return 0;
}

void *scheduling_module(void *arg) {
    // ... (your existing code for scheduling_module)

    pthread_mutex_lock(&job_queue_mutex);
    while (!exit_flag) {
        // Check the exit flag periodically
        // Continue normal processing
        pthread_cond_wait(&job_available_cond, &job_queue_mutex);
    }
    pthread_mutex_unlock(&job_queue_mutex);

    return NULL;
}

void *dispatching_module(void *arg) {
    while (1) {
    pthread_mutex_lock(&job_queue_mutex);
    // Check if there are jobs in the queue
    while (job_queue_head == NULL) {
        // If the queue is empty, wait for a signal
        pthread_cond_wait(&job_available_cond, &job_queue_mutex);
    }

    // Check if the exit flag is set
    if (exit_flag) {
        pthread_mutex_unlock(&job_queue_mutex);
        break; // Exit the loop and terminate the thread
    }

    // Get the next job from the queue
    struct Job *current_job = job_queue_head;
    job_queue_head = current_job->next;
    // Mark the job as running
    current_job->is_running = 1;
    pthread_mutex_unlock(&job_queue_mutex);
    // Execute the job using execv
    char *argv[] = {current_job->name, NULL};
    execv(current_job->name, argv);
    // If execv fails, print an error message
    perror("Error in execv");
    exit(EXIT_FAILURE);
}
}

// Function to list jobs (implement details)
void list_jobs() {
    pthread_mutex_lock(&job_queue_mutex);

    int num_jobs = 0;
    printf("Total number of jobs in the queue: ");
    printf("%d\n", get_num_jobs());
    printf("Scheduling Policy: FCFS.\n");
    printf("Name\tCPU_Time\tPri\tArrival_time\tProgress\n");

    struct Job *current = job_queue_head;
    current = job_queue_head;
    while (current != NULL) {
        printf("%s\t%d\t\t%d\t%ld.%06ld\t", current->name, current->cpu_time, current->priority,
               current->arrival_time.tv_sec, current->arrival_time.tv_usec);

        if (current->progress == 1) {
            printf("Run\n");
        } else {
            printf("\n");
        }

        current = current->next;
    }

    pthread_mutex_unlock(&job_queue_mutex);
}

// Function to display help information
void display_help() {
    printf("run <job> <time> <pri>: submit a job named <job>,\n");
    printf("                        execution time is <time>,\n");
    printf("                        priority is <pri>.\n");
    printf("list: display the job status.\n");
    printf("fcfs: change the scheduling policy to FCFS.\n");
    printf("sjf: change the scheduling policy to SJF.\n");
    printf("priority: change the scheduling policy to priority.\n");
    printf("test <benchmark> <policy> <num_of_jobs> <priority_levels>\n");
    printf("     <min_CPU_time> <max_CPU_time>\n");
    printf("quit: exit AUbatch\n");
}

// Helper function to get the number of jobs in the queue
int get_num_jobs() {
    pthread_mutex_lock(&job_queue_mutex);

    int num_jobs = 0;
    struct Job *current = job_queue_head;

    while (current != NULL) {
        num_jobs++;
        if (current->is_running) {
            num_jobs++; // Include the running job in the count
        }
        current = current->next;
    }

    pthread_mutex_unlock(&job_queue_mutex);
    return num_jobs;
}
