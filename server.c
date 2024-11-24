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

#define MAX_TEST_CASES 10
#define MAX_OUTPUT_LENGTH 100

pthread_mutex_t queue_lock;
pthread_mutex_t count_lock;
pthread_cond_t task_available;

char *test_case_solution_inputs[MAX_TEST_CASES];
char *test_case_solution_outputs[MAX_TEST_CASES];

int compare_test_case_output(char *compare_str, int index)
{
    return strcmp(test_case_solution_outputs[index], compare_str);
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
int count = 0;

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

void write_file(int sockfd)
{
    printf("Started writing file\n");
}

int get_count()
{
    pthread_mutex_lock(&count_lock);
    count++;
    pthread_mutex_unlock(&count_lock);
    return count;
}

int dec_count()
{
    pthread_mutex_lock(&count_lock);
    count--;
    pthread_mutex_unlock(&count_lock);
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
        printf("Compiling the program...");
        execlp(compiler, compiler, filename, output_file, executable, (char *)NULL);
        perror("Error executing gcc");
        exit(1);
    }
    else
    {
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
        {
            if (WEXITSTATUS(status) == 0)
            {
                printf("compilation succesfull! Executable: %s\n", executable);
            }
            else
            {
                printf("compilation failed with exit code: %d\n", WEXITSTATUS(status));
            }
        }
        else
        {
            printf("Compilation process did not terminate normally.\n");
        }
    }
}

char *run(char *executable, char *input)
{
    char *output;
    int pipe_fd[2];
    pid_t pid;
    char program[strlen(executable) + 2];
    char *result = (char *)malloc(BUFF_SIZE);

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
        ssize_t nbytes;

        // printf("Output from child process: \n");
        // while((nbytes = read(pipe_fd[0],buffer, BUFF_SIZE-1))>0){
        //     buffer[nbytes] = '\0';
        //     printf("%s", buffer);
        // }
        while (nbytes = read(pipe_fd[0], result, BUFF_SIZE - 1) > 0)
        {
            // result[nbytes] = '\0';
        }
        close(pipe_fd[0]);
    }
    return result;
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
        char *message = "success";

        bzero(buffer, BUFF_SIZE);

        int n;
        FILE *fp;
        char *cfile = "student_temp.c";
        // char cfile[50];
        // int client_count = get_count();
        // sprintf(cfile, "student_temp_%d.c", client_count);

        char *executable = "student_program";

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

        rwbytes = write(socket, message, strlen(message));
        if (rwbytes < 0)
        {
            perror("Write Error");
            close(socket);
            pthread_exit(NULL);
        }
        remove(cfile);
        remove(executable);
        dec_count();
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
    char *executable = "solution";
    compile(solution_file, executable);

    FILE *input_test_cases_file = fopen(test_cases_file, "r");
    if (input_test_cases_file == NULL)
    {
        printf("Error in testcase file reading");
        exit(1);
    }

    int test_case_count = 0;
    char current_line[MAX_OUTPUT_LENGTH];
    while (test_case_count < MAX_TEST_CASES && fgets(current_line, MAX_OUTPUT_LENGTH, input_test_cases_file))
    {
        current_line[strcspn(current_line, "\n")] = '\0';
        char *temp_result = run(executable, current_line);
        printf("output for: %s is %s\n", current_line, temp_result);
        test_case_solution_outputs[test_case_count] = temp_result;
        test_case_count++;
    }

    for (int i = 0; i < test_case_count; i++)
    {
        printf("%s\n", test_case_solution_outputs[i]);
    }

    pthread_mutex_init(&queue_lock, NULL);
    pthread_mutex_init(&count_lock, NULL);
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
    pthread_mutex_destroy(&count_lock);
    pthread_cond_destroy(&task_available);
    return 0;
}
