#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <pthread.h>
#include <fcntl.h>
#define BUFF_SIZE 1024

#define RESET "\033[0m"   // Reset to default color
#define GREEN "\033[32m"  // Green text
#define RED "\033[31m"    // Red text
#define YELLOW "\033[33m" // Yellow text

#define MAX_TEST_CASES 10
#define MAX_OUTPUT_LENGTH 100

pthread_mutex_t queue_lock;
pthread_mutex_t client_count_lock;
pthread_cond_t task_available;

char test_case_solution_inputs[MAX_TEST_CASES][MAX_OUTPUT_LENGTH];
char test_case_solution_outputs[MAX_TEST_CASES][MAX_OUTPUT_LENGTH];
int test_case_count = 0;
int client_count = 0;

int get_client_count()
{
    pthread_mutex_lock(&client_count_lock);
    int temp = client_count;
    client_count++;
    pthread_mutex_unlock(&client_count_lock);
    return temp;
}

int compare_test_case_output(char *compare_str, int index)
{
    if (strcmp(test_case_solution_outputs[index], compare_str) == 0)
    {
        return 1;
    }
    return 0;
}
typedef struct TaskNode
{
    int *socket;
    struct TaskNode *next;
} TaskNode;

typedef struct
{
    TaskNode *front;
    TaskNode *rear;
    int size;
} TaskQueue;

TaskQueue queue;

TaskNode *create_task_node(int *socket)
{
    TaskNode *new_node = (TaskNode *)malloc(sizeof(TaskNode));
    new_node->socket = socket;
    new_node->next = NULL;
    return new_node;
}

void enqueue_task(int *arg)
{
    TaskNode *new_node = create_task_node(arg);
    pthread_mutex_lock(&queue_lock);
    if (queue.rear == NULL)
    {
        queue.front = queue.rear = new_node;
    }
    else
    {
        queue.rear->next = new_node;
        queue.rear = new_node;
    }
    queue.size++;
    pthread_cond_signal(&task_available);
    pthread_mutex_unlock(&queue_lock);
}

int *dequeue_task()
{
    pthread_mutex_lock(&queue_lock);
    while (queue.size == 0)
    {
        pthread_cond_wait(&task_available, &queue_lock);
    }
    TaskNode *temp = queue.front;
    int *socket = temp->socket;
    queue.front = queue.front->next;
    if (queue.front == NULL)
    {
        queue.rear = NULL;
    }
    free(temp);
    queue.size--;
    pthread_mutex_unlock(&queue_lock);
    return socket;
}

void compile(char *filename, char *executable)
{
    char *compiler = "/usr/bin/gcc";
    char *output_file = "-o";
    int pid = fork();
    int status;
    if (pid < 0)
    {
        perror("Fork Failed");
        exit(1);
    }
    else if (pid == 0)
    {
        execlp(compiler, compiler, filename, output_file, executable, (char *)NULL);
        perror("Error executing gcc");
        exit(1);
    }
    else
    {
        waitpid(pid, &status, 0);
    }
}

char *run(char *executable, char *input)
{
    char *output = (char *)malloc(MAX_OUTPUT_LENGTH * sizeof(char));
    int pipe_fd[2];
    pid_t pid;
    char program[100];

    sprintf(program, "./%s", executable);
    if (pipe(pipe_fd) == -1)
    {
        perror("pipe");
        exit(1);
    }

    pid = fork();
    if (pid < 0)
    {
        perror("fork error");
        exit(1);
    }
    else if (pid == 0)
    {
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]);

        char *args[] = {program, input, NULL};
        execv(args[0], args);

        perror("execv fail");
        exit(1);
    }
    else
    {
        close(pipe_fd[1]);
        int status;
        waitpid(pid, &status, 0);
        ssize_t nbytes;
        nbytes = read(pipe_fd[0], output, MAX_OUTPUT_LENGTH - 1);
        if (nbytes > 0)
        {
            output[nbytes] = '\0';
        }
        else
        {
            strcpy(output, "");
        }
        close(pipe_fd[0]);
    }
    return output;
}

void *thread_work()
{
    while (1)
    {
        int *new_socket = dequeue_task();
        int socket = *((int *)new_socket);
        free(new_socket);

        int rwbytes;
        char buffer[BUFF_SIZE] = {0};
        char result[MAX_OUTPUT_LENGTH];

        bzero(buffer, BUFF_SIZE);

        int n;
        int client_number = get_client_count();
        FILE *fp;
        time_t t = time(NULL);
        char cfile[100];
        snprintf(cfile, sizeof(cfile), "student_program_%ld_%d.c", t, client_number);

        t = time(NULL);
        char executable[100];
        snprintf(executable, sizeof(executable), "student_program_%ld_%d.o", t, client_number);

        fp = fopen(cfile, "w");
        if (fp == NULL)
        {
            printf("Error in file creation\n");
            exit(1);
        }
        while (1)
        {
            n = recv(socket, buffer, BUFF_SIZE, 0);
            if (n <= 0)
            {
                break;
            }
            fprintf(fp, "%s", buffer);
            bzero(buffer, BUFF_SIZE);
        }
        fclose(fp);
        char command[128];

        compile(cfile, executable);
        int success_count = 0;
        char *client_results[MAX_TEST_CASES];
        for (int i = 0; i < test_case_count; i++)
        {
            client_results[i] = run(executable, test_case_solution_inputs[i]);
            if (compare_test_case_output(client_results[i], i))
            {
                success_count++;
            }
        }
        sprintf(result, YELLOW " %.2f %%"
                               ", " RED "(" GREEN "%d" RED "/%d)" RESET " test cases passed!",
                (success_count / (float)test_case_count) * 100, success_count, test_case_count);
        rwbytes = write(socket, result, strlen(result));
        if (rwbytes < 0)
        {
            perror("Write Error");
            close(socket);
            pthread_exit(NULL);
        }
        remove(cfile);
        remove(executable);
        close(socket);
    }
}

void handle_task(int new_socket)
{
    int *client_sock = malloc(sizeof(int));
    if (client_sock == NULL)
    {
        printf("Memory allocation fail\n");
        close(new_socket);
        return;
    }
    *client_sock = new_socket;
    enqueue_task(client_sock);
}

void reset_queue()
{
    pthread_mutex_init(&queue_lock, NULL);
    queue.front = NULL;
    queue.rear = NULL;
    queue.size = 0;
}

int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        printf("usage: ./filename <port> <thread_count> <solution.c> <test_cases.txt>\n");
        exit(1);
    }

    int port = atoi(argv[1]);
    int thread_count = atoi(argv[2]);
    char *solution_file = argv[3];
    char *test_cases_file = argv[4];
    char *executable = "solution.o";
    compile(solution_file, executable);

    FILE *input_test_cases_file = fopen(test_cases_file, "r");
    if (input_test_cases_file == NULL)
    {
        printf("Error in testcase file reading");
        exit(1);
    }
    while (test_case_count < MAX_TEST_CASES && fgets(test_case_solution_inputs[test_case_count], MAX_OUTPUT_LENGTH, input_test_cases_file))
    {
        test_case_solution_inputs[test_case_count][strcspn(test_case_solution_inputs[test_case_count], "\n")] = '\0';
        char *temp_result = run(executable, test_case_solution_inputs[test_case_count]);
        strcpy(test_case_solution_outputs[test_case_count], temp_result);
        free(temp_result);
        test_case_count++;
    }

    pthread_mutex_init(&queue_lock, NULL);
    pthread_mutex_init(&client_count_lock, NULL);
    pthread_cond_init(&task_available, NULL);

    int main_socket, new_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    reset_queue();

    if ((main_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Socket creation error\n");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(main_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Binding Failed.");
        exit(1);
    }

    if (listen(main_socket, 10) < 0)
    {
        perror("Listening Failed.");
        exit(1);
    }

    pthread_t threads[thread_count];
    for (int i = 0; i < thread_count; i++)
    {
        pthread_create(&threads[i], NULL, &thread_work, NULL);
    }

    while (1)
    {
        if ((new_socket = accept(main_socket, (struct sockaddr *)&client_addr, &client_len)) < 0)
        {
            printf("Accept failed.");
            continue;
        }
        handle_task(new_socket);
    }
    close(main_socket);
    pthread_mutex_destroy(&queue_lock);
    pthread_mutex_destroy(&client_count_lock);
    pthread_cond_destroy(&task_available);
    return 0;
}
