#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#define BUFF_SIZE 1024

pthread_mutex_t queue_lock;
pthread_cond_t task_available;

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

void write_file(int sockfd)
{
    printf("Started writing file\n");
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
        char *filename = "server_temp_file.c";
        fp = fopen(filename, "w");
        if (fp == NULL)
        {
            printf("Error in file creation\n");
            exit(1);
        }
        while (1)
        {
            printf("Waiting to receive.\n");
            n = recv(socket, buffer, BUFF_SIZE, 0);
            printf("recieved %d bytes.\n", n);
            if (n <= 0)
            {
                break;
            }
            fprintf(fp, "%s", buffer);
            bzero(buffer, BUFF_SIZE);
        }

        rwbytes = write(socket, message, strlen(message));
        if (rwbytes < 0)
        {
            perror("Write Error");
            close(socket);
            pthread_exit(NULL);
        }
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
    if (argc < 3)
    {
        printf("usage: ./filename <port> <thread_count>\n");
        exit(1);
    }

    int port = atoi(argv[1]);
    int thread_count = atoi(argv[2]);

    pthread_mutex_init(&queue_lock, NULL);
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
    pthread_cond_destroy(&task_available);
    return 0;
}
