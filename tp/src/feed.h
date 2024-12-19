#ifndef FEED_H
#define FEED_H

#include "utils.h"

typedef struct
{
    int id;
    char username[MAX_USER_NAME];
    char subscribed_topics[MAX_TOPICS][MAX_TOPIC_NAME];
    int subscription_count;
    int is_connected;
} User;

typedef struct
{
    char topic[MAX_TOPIC_NAME];
    User autor;
    char message[MAX_MESSAGE_LENGHT];
    int expiration_time; // 0 se for não-persistente
} Message;

typedef struct
{
    char name[MAX_TOPIC_NAME];
    Message messages[MAX_PERSISTENT];
    int message_count;
    TopicState state; // bloqueado ou não bloqueado
} Topic;

typedef struct
{
    User user;
    char fifo_pessoal[FIFO_NAME_LENGHT];
    bool running;
    pthread_mutex_t mutex;
} Feed;

void verify_manager();
void init_user(User *user, const char *username);
int send_info(User *user);
int receive_log_status(User *user);
void *sender_thread(void *arg);
void *receiver_thread(void *arg);

#endif
