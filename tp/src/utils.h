#ifndef UTILS 
#define UTILS

//includes

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>



// define dos sizes

#define MAX_USERS 10
#define MAX_TOPICS 20
#define MAX_PERSISTENT 5
#define MAX_TOPIC_NAME 20
#define MAX_MESSAGE_LENGHT 300
#define FIFO_NAME_LENGHT 100 //não obrigatório
#define MAX_USER_NAME 50

#define FIFO_GERAL "fifo_geral"
#define FIFO_PESSOAL "fifo_%s"


//structs 

typedef enum {
    TOPIC_UNLOCKED,
    TOPIC_LOCKED

} TopicState;

typedef enum {
    MESSAGE_TYPE_CONN, // Representa uma nova conexão
    MESSAGE_TYPE_CMD   // Representa um comando de cliente
} MessageType;



#endif 