// Jackson Shaw
// COMP 7500 OS3 Project
// Compiled via Makefile. Run "make" in terminal, then "./aubatch" to start program
// To run a micro benchmark, after compiling via "make", run "./batch_job <time>",
// where time is replaced with desired time of run
// Code is loosely based off of Dr. Qin model, but is largely my own with some Struct, formatting, commenting, and logic help
// via ChatGPT

// Notes for testing/grading
// My code should pass all of the Basics and things in the "run" command section.
// Regarding the test command section, to model my code similar to what was presented in the project specification,
// the user will not be able to call list during a test benchmark. The statistics from the job run will print at the end
// of the batch (when the last job finishes and exits queue). If doing a test with 10-20 jobs and more than 5sec CPU time each
// you may have to wait for a little bit for the benchmark to finish (it is not frozen). My reasoning behind not allowing
// the user to run other commands while a benchmark is running is that if the user were to add a job or change policy while
// a benchmark is running then the statistics would be tainted. For best results, do not run jobs before each benchmark.
// multiple benchmarks can be run one after another as the statistics will reset once a benchmark is completed.
// If you have any questions on how my code is run/designed/executed, please contact me at jps0096@auburn.edu

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

// Define a structure to represent a job
struct Job
{
    char *name;
    int cpu_time;
    int priority;
    struct timeval arrival_time;
    int progress; // 0: Not started, 1: Run
    int is_running;
    struct Job *next; // Pointer to the next job in the linked list
    struct Job *tail; // Pointer to tail
};

int current_scheduling_policy = 0; // 0: FCFS, 1: SJF, 2: Priority
int total_jobs_submitted = 0;
double total_turnaround_time = 0.0;
double total_cpu_time = 0.0;
double total_waiting_time = 0.0;
struct timeval program_start_time;

// Synchronization objects
pthread_mutex_t job_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t job_available_cond = PTHREAD_COND_INITIALIZER;

// Shared flag to signal threads to exit
int exit_flag = 0;

// Head of the linked list representing the job queue
struct Job *job_queue_head = NULL;
struct Job *job_queue_tail = NULL;

// Function prototypes
void *scheduling_module(void *arg);
void *dispatching_module(void *arg);
void list_jobs();
void display_help(); // Added function for help
const char *get_scheduling_policy();
struct Job *find_completed_job(const char *job_name);

void submit_job(const char *job_name_p, int execution_time_p, int priority_p)
{
    // Create a new job and add it to the job queue
    // Extract arguments for the "run" command
    char *job_name = strdup(job_name_p);
    int execution_time = execution_time_p;
    int priority = priority_p;

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
    if (job_queue_head == NULL)
    {
        new_job->is_running = 1;
    }

    pthread_mutex_lock(&job_queue_mutex);
    // Insert the new job at the end of the linked list
    if (job_queue_head == NULL)
    {
        job_queue_head = new_job;
    }
    else
    {
        struct Job *current = job_queue_head;
        while (current->next != NULL)
        {
            current = current->next;
        }
        current->next = new_job;
    }
    // Update the tail pointer
    if (job_queue_tail == NULL)
    {
        // If the queue is empty, set both head and tail to the new job
        job_queue_head = new_job;
        job_queue_tail = new_job;
    }
    else
    {
        // If the queue is not empty, update the tail to point to the new job
        job_queue_tail->next = new_job;
        job_queue_tail = new_job;
    }

    pthread_cond_signal(&job_available_cond);
    pthread_mutex_unlock(&job_queue_mutex);

    // Provide feedback for job submission
    printf("Job %s was submitted.\n", job_name);
    printf("Total number of jobs in the queue: %d\n", get_num_jobs());
    printf("Scheduling Policy: %s.\n", get_scheduling_policy());
}

void submit_job_for_benchmark(struct Job *new_job)
{

    // If this is the only job in the queue, consider it as running
    if (job_queue_head == NULL)
    {
        new_job->is_running = 1;
    }

    pthread_mutex_lock(&job_queue_mutex);
    // Insert the new job at the end of the linked list
    if (job_queue_head == NULL)
    {
        job_queue_head = new_job;
    }
    else
    {
        struct Job *current = job_queue_head;
        while (current->next != NULL)
        {
            current = current->next;
        }
        current->next = new_job;
    }
    // Update the tail pointer
    if (job_queue_tail == NULL)
    {
        // If the queue is empty, set both head and tail to the new job
        job_queue_head = new_job;
        job_queue_tail = new_job;
    }
    else
    {
        // If the queue is not empty, update the tail to point to the new job
        job_queue_tail->next = new_job;
        job_queue_tail = new_job;
    }

    pthread_cond_signal(&job_available_cond);
    pthread_mutex_unlock(&job_queue_mutex);
}

int main()
{
    pthread_t scheduling_thread, dispatching_thread;
    if (pthread_create(&scheduling_thread, NULL, scheduling_module, NULL) != 0)
    {
        perror("Error creating scheduling thread");
        exit(EXIT_FAILURE);
    }

    // Create and start the dispatching thread
    if (pthread_create(&dispatching_thread, NULL, dispatching_module, NULL) != 0)
    {
        perror("Error creating dispatching thread");
        exit(EXIT_FAILURE);
    }

    // Detach the threads
    if (pthread_detach(scheduling_thread) != 0)
    {
        perror("Error detaching scheduling thread");
        exit(EXIT_FAILURE);
    }

    if (pthread_detach(dispatching_thread) != 0)
    {
        perror("Error detaching dispatching thread");
        exit(EXIT_FAILURE);
    }

    // Initialize job_queue_mutex
    pthread_mutex_init(&job_queue_mutex, NULL);

    // Welcome message and prompt
    printf("Welcome to Jackson's batch job scheduler Version 1.0\n");
    printf("Type 'help' to find more about AUbatch commands.\n");
    printf("> ");

    // Command execution loop
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), stdin) != NULL)
    {
        // Parse the command
        char *command = strtok(buffer, " \n");

        if (strcmp(command, "help") == 0)
        {
            // Display help information
            display_help();
        }
        else if (strcmp(command, "run") == 0)
        {
            // Check the number of arguments
            char *tokens[3];
            int num_args = 0;

            char *token = strtok(NULL, " ");
            while (token != NULL && num_args < 3)
            {
                tokens[num_args++] = token;
                token = strtok(NULL, " ");
            }

            // Ensure that the correct number of arguments is provided
            if (num_args != 3)
            {
                printf("Error: Invalid number of arguments for the 'run' command.\n");
                printf("Usage: run <job_name> <execution_time> <priority>\n");
                printf("> "); // Prompt for the next command
                continue;
            }

            // Now, you can use tokens[0], tokens[1], and tokens[2] as your arguments
            char *job_name = tokens[0];
            char *execution_time_str = tokens[1];
            char *priority_str = tokens[2];

            // Convert string arguments to integers
            int execution_time = atoi(execution_time_str);
            int priority = atoi(priority_str);

            // Now, you can use job_name, execution_time, and priority safely
            submit_job(job_name, execution_time, priority);
        }

        else if (strcmp(command, "list") == 0)
        {
            // Display queue information
            list_jobs();
        }
        else if (strcmp(command, "fcfs") == 0)
        {
            current_scheduling_policy = 0;
            schedule_fcfs();
        }
        else if (strcmp(command, "sjf") == 0)
        {
            current_scheduling_policy = 1;
            schedule_sjf();
        }
        else if (strcmp(command, "priority") == 0)
        {
            current_scheduling_policy = 2;
            schedule_priority();
        }
        else if (strcmp(command, "test") == 0)
        {
            // Check the number of arguments
            char *tokens[6];
            int num_args = 0;

            char *token = strtok(NULL, " ");
            while (token != NULL && num_args < 6)
            {
                tokens[num_args++] = token;
                token = strtok(NULL, " ");
            }

            // Ensure that the correct number of arguments is provided
            if (num_args != 6)
            {
                printf("Error: Invalid number of arguments for the 'test' command.\n");
                printf("Usage: test <benchmark> <policy> <num_of_jobs> <priority_levels> <min_CPU_time> <max_CPU_time>\n");
                printf("> "); // Prompt for the next command
                continue;
            }

            // Now, you can use tokens[0] to tokens[5] as your arguments
            char *benchmark_name = tokens[0];
            char *policy = tokens[1];
            int num_jobs = atoi(tokens[2]);
            int priority_levels = atoi(tokens[3]);
            int min_cpu_time = atoi(tokens[4]);
            int max_cpu_time = atoi(tokens[5]);

            // Now, you can use the extracted values as needed
            // Validate input parameters if needed
            test_command(benchmark_name, policy, num_jobs, priority_levels, min_cpu_time, max_cpu_time);
        }

        else if (strcmp(command, "quit") == 0)
        {
            // Set the exit flag and signal threads to wake up
            exit_flag = 1;
            pthread_cond_signal(&job_available_cond);
            // pthread_join(scheduling_thread, NULL);
            // pthread_join(dispatching_thread, NULL);
            total_jobs_submitted += get_num_waiting_jobs();
            printf("Total number of job submitted: %d\n", total_jobs_submitted);
            printf("Average turnaround time: %.2f seconds\n", total_turnaround_time / total_jobs_submitted);
            printf("Average CPU time: %.2f seconds\n", total_cpu_time / total_jobs_submitted);
            printf("Average waiting time: %.2f seconds\n", total_waiting_time / total_jobs_submitted);
            printf("Throughput: %.3f No./second\n", total_jobs_submitted / total_turnaround_time);

            // Free the memory allocated for the linked list
            struct Job *current = job_queue_head;
            struct Job *next;
            while (current != NULL)
            {
                next = current->next;
                free(current->name);
                free(current);
                current = next;
            }

            break;
        }
        else
        {
            // Handle unknown command or errors
            printf("Unknown command. Type 'help' for more information.\n");
        }

        printf("> "); // Prompt for the next command
    }

    return 0;
}

void *scheduling_module(void *arg)
{
    while (1)
    {
        pthread_mutex_lock(&job_queue_mutex);
        while (job_queue_head == NULL)
        {
            pthread_cond_wait(&job_available_cond, &job_queue_mutex);
        }

        if (exit_flag)
        {
            pthread_mutex_unlock(&job_queue_mutex);
            break; // Exit the loop and terminate the thread
        }

        struct Job *current_job = job_queue_head;
        // Check if the job has been marked as completed (progress == 1)
        if (current_job->progress == 1)
        {
            // Remove the completed job from the queue
            job_queue_head = current_job->next;
            free(current_job->name);
            free(current_job);
            pthread_mutex_unlock(&job_queue_mutex);
        }
        else
        {
            current_job->is_running = 1;
            pthread_mutex_unlock(&job_queue_mutex);
            pthread_cond_signal(&job_available_cond);

            // You can implement your scheduling logic here
            // For example, if you are using FCFS, SJF, or Priority scheduling,
            // you can call the corresponding functions.
            // Implement scheduling logic based on the current policy
            if (current_scheduling_policy == 0)
            { // FCFS
                sort_job_queue_by_arrival_time();
            }
            else if (current_scheduling_policy == 1)
            { // SJF
                sort_job_queue_by_cpu_time();
            }
            else if (current_scheduling_policy == 2)
            { // Priority
                sort_job_queue_by_priority();
            }
        }
    }

    return NULL;
}

void *dispatching_module(void *arg)
{
    while (1)
    {
        pthread_mutex_lock(&job_queue_mutex);

        // Check if there are jobs in the queue
        while (job_queue_head == NULL)
        {
            // If the queue is empty, wait for a signal
            pthread_cond_wait(&job_available_cond, &job_queue_mutex);
        }

        // Check if the exit flag is set
        if (exit_flag)
        {
            pthread_mutex_unlock(&job_queue_mutex);
            break; // Exit the loop and terminate the thread
        }

        // Get the next job from the queue
        struct Job *current_job = job_queue_head;

        // Check if the job has already been completed (progress == 1)
        if (current_job->progress == 1)
        {
            // Remove the completed job from the queue
            job_queue_head = current_job->next;
            free(current_job->name);
            free(current_job);
            pthread_mutex_unlock(&job_queue_mutex);
            continue; // Skip the rest of the loop and move to the next iteration
        }
        total_jobs_submitted++;
        // Update statistics
        struct timeval current_time;
        gettimeofday(&current_time, NULL);
        // Update the statistics (assuming the job was submitted successfully)

        double turnaround_time = (current_time.tv_sec - current_job->arrival_time.tv_sec) +
                                 (current_time.tv_usec - current_job->arrival_time.tv_usec) * 1e-6;

        double waiting_time = turnaround_time - current_job->cpu_time;
        total_turnaround_time += turnaround_time;
        total_cpu_time += current_job->cpu_time;
        total_waiting_time += (waiting_time > 0) ? waiting_time : 0;

        // Mark the job as running before unlocking the mutex
        current_job->is_running = 1;
        pthread_mutex_unlock(&job_queue_mutex);

        // Fork a child process for job execution
        pid_t child_pid = fork();

        if (child_pid == -1)
        {
            perror("Error in fork");
            exit(EXIT_FAILURE);
        }

        if (child_pid == 0)
        { // Child process
            // Execute the job using system
            // Execute the job using system, passing the CPU time as a parameter
            char command[1024];
            snprintf(command, sizeof(command), "./batch_job %d", current_job->cpu_time);
            int result = system(command);
            // Update the progress based on the result of system
            if (result == 0)
            {
                current_job->progress = 1; // Set progress to 1 for successful execution
            }
            else
            {
                current_job->progress = 0; // Set progress to 0 for failed execution
            }

            // Notify the main thread that a job has finished
            pthread_cond_signal(&job_available_cond);

            _exit(EXIT_SUCCESS);
        }
        else
        { // Parent process
            // Wait for the child process to complete
            int status;
            waitpid(child_pid, &status, 0);

            // Update the progress based on the result of the child process
            if (WIFEXITED(status))
            {
                if (WEXITSTATUS(status) == 0)
                {
                    current_job->progress = 1; // Set progress to 1 for successful execution
                }
                else
                {
                    current_job->progress = 0; // Set progress to 0 for failed execution
                }
            }
            else
            {
                perror("Error in child process");
                current_job->progress = 0; // Set progress to 0 for unknown errors
            }

            // Notify the main thread that a job has been completed
            pthread_cond_signal(&job_available_cond);
        }
    }

    return NULL;
}

void test_command(const char *benchmark_name, const char *policy, int num_jobs, int priority_levels, int min_cpu_time, int max_cpu_time)
{
    struct Job *head = NULL;
    struct Job *tail = NULL;

    if (strcmp(policy, "fcfs") == 0)
    {
        current_scheduling_policy = 0;
    }
    else if (strcmp(policy, "sjf") == 0)
    {
        current_scheduling_policy = 1;
    }
    else if (strcmp(policy, "priority") == 0)
    {
        current_scheduling_policy = 2;
    }
    else
    {
        printf("Incorrect scheduling policy. Please enter <fcfs>, <sjf>, or <priority>\n");
        return;
    }

    // Store the total start time for waiting time calculation
    struct timeval total_start_time;
    gettimeofday(&total_start_time, NULL);

    int i;
    for (i = 1; i <= num_jobs; ++i)
    {
        char job_name[16];
        snprintf(job_name, sizeof(job_name), "%s_%d", benchmark_name, i);

        int execution_time = rand() % (max_cpu_time - min_cpu_time + 1) + min_cpu_time;
        int priority = rand() % priority_levels + 1;

        // Create a new job and add it to the linked list
        struct Job *simulated_job = (struct Job *)malloc(sizeof(struct Job));
        simulated_job->name = strdup(job_name);
        simulated_job->cpu_time = execution_time;
        simulated_job->priority = priority;
        gettimeofday(&simulated_job->arrival_time, NULL);
        simulated_job->progress = 0;
        simulated_job->is_running = 0;
        simulated_job->next = NULL;
        submit_job_for_benchmark(simulated_job);
    }
    // wait until all jobs finish. then print stats and reset vals for next benchmark
    while (job_queue_head != NULL)
    {
    }
    printf("Total number of job submitted: %d\n", total_jobs_submitted);
    printf("Average turnaround time: %.2f seconds\n", total_turnaround_time / total_jobs_submitted);
    printf("Average CPU time: %.2f seconds\n", total_cpu_time / total_jobs_submitted);
    printf("Average waiting time: %.2f seconds\n", total_waiting_time / total_jobs_submitted);
    printf("Throughput: %.3f No./second\n", total_jobs_submitted / total_turnaround_time);

    current_scheduling_policy = 0; // 0: FCFS, 1: SJF, 2: Priority
    total_jobs_submitted = 0;
    total_turnaround_time = 0.0;
    total_cpu_time = 0.0;
    total_waiting_time = 0.0;
}

// to get policy
const char *get_scheduling_policy()
{
    switch (current_scheduling_policy)
    {
    case 0:
        return "FCFS";
    case 1:
        return "SJF";
    case 2:
        return "Priority";
    default:
        return "Unknown Policy";
    }
}

// Function to list jobs (implement details)
void list_jobs()
{
    int num_jobs = 0;
    printf("Total number of jobs in the queue: %d\n", get_num_jobs());
    printf("Scheduling Policy: %s.\n", get_scheduling_policy());
    printf("Name\tCPU_Time\tPri\tArrival_time\t\tStatus\n");

    struct Job *current = job_queue_head;
    while (current != NULL)
    {
        printf("%s\t%d\t\t%d\t%ld.%06ld\t", current->name, current->cpu_time, current->priority,
               current->arrival_time.tv_sec, current->arrival_time.tv_usec);

        if (current->is_running)
        {
            printf("Running\n");
        }
        else if (current->progress == 1)
        {
            printf("Completed\n");
        }
        else
        {
            printf("Queued\n");
        }

        current = current->next;
    }
}

// Function to display help information
void display_help()
{
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
int get_num_jobs()
{

    int num_jobs = 0;
    struct Job *current = job_queue_head;

    while (current != NULL)
    {
        num_jobs++;
        current = current->next;
    }

    return num_jobs;
}

int get_num_waiting_jobs()
{
    int num_jobs = 0;
    struct Job *current = job_queue_head;

    while (current != NULL)
    {
        if (!current->is_running)
        {
            num_jobs++;
        }
        current = current->next;
    }

    return num_jobs;
}

void schedule_priority()
{
    pthread_mutex_lock(&job_queue_mutex);
    // Sort the job queue based on priority (highest priority first)
    sort_job_queue_by_priority();
    printf("Scheduling policy is switched to Priority. All the %d waiting jobs have been rescheduled.\n", get_num_waiting_jobs());
    pthread_mutex_unlock(&job_queue_mutex);
}

void sort_job_queue_by_priority()
{
    struct Job *node = NULL, *temp = NULL;
    struct Job temp_job; // Temp variable to store the entire job
    node = job_queue_head;
    if (node == NULL || node->next == NULL)
    {
        return;
    }

    // If the first job is running and there are only two jobs, no need to reschedule
    if (node->is_running && node->next->next == NULL)
    {
        return;
    }

    if (node->is_running && node->next != NULL)
    {
        node = node->next;
    }

    while (node != NULL)
    {
        temp = node;
        while (temp->next != NULL) // Travel till the second last element
        {
            if (temp->priority < temp->next->priority) // Compare the priority of the nodes
            {
                // Swap all the values in the Job struct
                swap_jobs(temp, temp->next);
            }
            temp = temp->next; // Move to the next element
        }
        node = node->next; // Move to the next node
    }
}

void schedule_sjf()
{
    pthread_mutex_lock(&job_queue_mutex);
    // Sort the job queue based on CPU time (shortest job first)
    sort_job_queue_by_cpu_time();
    printf("Scheduling policy is switched to SJF. All the %d waiting jobs have been rescheduled.\n", get_num_waiting_jobs());
    pthread_mutex_unlock(&job_queue_mutex);
}

void sort_job_queue_by_cpu_time()
{
    struct Job *node = NULL, *temp = NULL;
    struct Job temp_job; // Temp variable to store the entire job
    node = job_queue_head;
    if (node == NULL || node->next == NULL)
    {
        return;
    }

    // If the first job is running and there are only two jobs, no need to reschedule
    if (node->is_running && node->next->next == NULL)
    {
        return;
    }

    if (node->is_running && node->next != NULL)
    {
        node = node->next;
    }

    while (node != NULL)
    {
        temp = node;
        while (temp->next != NULL) // Travel till the second last element
        {
            if (temp->cpu_time > temp->next->cpu_time) // Compare the priority of the nodes
            {
                // Swap all the values in the Job struct
                swap_jobs(temp, temp->next);
            }
            temp = temp->next; // Move to the next element
        }
        node = node->next; // Move to the next node
    }
}

void swap_jobs(struct Job *job1, struct Job *job2)
{
    // Swap the attributes of two jobs, including the name
    char *temp_name = (job1->name);
    // free(job1->name);
    job1->name = (job2->name);
    // free(job2->name);
    job2->name = temp_name;

    // Swap other job attributes
    int temp_cpu_time = job1->cpu_time;
    int temp_priority = job1->priority;
    struct timeval temp_arrival_time = job1->arrival_time;
    int temp_progress = job1->progress;
    int temp_is_running = job1->is_running;

    job1->cpu_time = job2->cpu_time;
    job1->priority = job2->priority;
    job1->arrival_time = job2->arrival_time;
    job1->progress = job2->progress;
    job1->is_running = job2->is_running;

    job2->cpu_time = temp_cpu_time;
    job2->priority = temp_priority;
    job2->arrival_time = temp_arrival_time;
    job2->progress = temp_progress;
    job2->is_running = temp_is_running;
}

void schedule_fcfs()
{
    pthread_mutex_lock(&job_queue_mutex);
    // Reorganize the job queue based on the order of arrival (FCFS)
    sort_job_queue_by_arrival_time();
    printf("Scheduling policy is switched to FCFS. All the %d waiting jobs have been rescheduled.\n", get_num_waiting_jobs());
    pthread_mutex_unlock(&job_queue_mutex);
}

void sort_job_queue_by_arrival_time()
{
    struct Job *node = NULL, *temp = NULL;
    struct Job temp_job; // Temp variable to store the entire job
    node = job_queue_head;
    if (node == NULL || node->next == NULL)
    {
        return;
    }

    // If the first job is running and there are only two jobs, no need to reschedule
    if (node->is_running && node->next->next == NULL)
    {
        return;
    }

    if (node->is_running && node->next != NULL)
    {
        node = node->next;
    }

    while (node != NULL)
    {
        temp = node;
        while (temp->next != NULL) // Travel till the second last element
        {
            if (temp->arrival_time.tv_sec > temp->next->arrival_time.tv_sec ||
                temp->arrival_time.tv_sec == temp->next->arrival_time.tv_sec &&
                    temp->arrival_time.tv_usec > temp->next->arrival_time.tv_usec) // Compare the priority of the nodes
            {
                // Swap all the values in the Job struct
                swap_jobs(temp, temp->next);
            }
            temp = temp->next; // Move to the next element
        }
        node = node->next; // Move to the next node
    }
}
