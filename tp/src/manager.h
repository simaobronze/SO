#ifndef MANAGER_H
#define MANAGER_H

#include "utils.h"
#include "feed.h"

struct Manager;

typedef struct
{
    User users[MAX_USERS];
    int user_count;
    Topic topics[MAX_TOPICS];
    int topic_count;
    pthread_mutex_t mutex;
} Manager;

extern Manager *global_manager;

typedef struct
{
    Manager *manager;
    char username[MAX_USER_NAME];
    char fifo_pessoal[FIFO_NAME_LENGHT];
} ClientData;

typedef struct
{
    Manager *manager;
    int topic_index;
    int message_index;
} TimerThreadArgs;

void createFifoGeral();
int valida_user(Manager *manager, const char *username);
void add_user(Manager *manager, const char *username);
void command_handler(const char *command, ClientData *client_data, const char *username);
int remove_user(Manager *manager, const char *username);
int subscribe_to_topic(const char *topic_name, ClientData *clientData);
int unsubscribe_from_topic(const char *topic_name, Manager *manager);
int send_message(Message *message, ClientData *clientData);
int show_topics(Manager *manager);
void distribute_message_to_subscribers(Message *message, Manager *manager);
void *start_message_timer_thread(void *args);
void remove_expired_message(Manager *manager, int topic_index, int message_index);
int add_message_to_manager(Manager *manager, Message *message);
int list_users(Manager *manager);
int list_topic_messages(Manager *manager, const char *topic_name);

#endif