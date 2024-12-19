#include "manager.h"

Manager *global_manager = NULL;

void createFifoGeral()
{

    if (access(FIFO_GERAL, F_OK) != 0)
    {
        printf("FIFO geral não existe. Criando...\n");
        if (mkfifo(FIFO_GERAL, 0760) == -1)
        {
            printf("Erro ao criar o FIFO geral");
            exit(1);
        }
    }
    // a funcionar
}

void *clientHandler(void *pdata)
{
    Manager *manager = (Manager *)pdata;

    while (1)
    {
        printf("Handler a funcionar.\n");

        int fdl = open(FIFO_GERAL, O_RDWR);
        if (fdl == -1)
        {
            printf("Erro ao abrir FIFO geral.\n");
            pthread_exit(NULL);
        }

        char buffer[512];
        char username[MAX_USER_NAME];

        ssize_t nbytes = read(fdl, buffer, sizeof(buffer) - 1);
        if (nbytes == 0)
        {
            printf("FIFO geral fechado.\n");
            close(fdl);
            exit(2);
        }
        else if (nbytes < 0)
        {
            printf("Erro ao ler do FIFO geral.\n");
            close(fdl);
            continue;
        }

        buffer[nbytes] = '\0';

        int msg_type;
        char content[512];
        sscanf(buffer, "%d:%511[^:]:%511[^\n]", &msg_type, content, username);
        printf("Mensagem recebida no FIFO geral (tipo %d): %s : %s\n", msg_type, content, username);

        if (msg_type == MESSAGE_TYPE_CONN)
        {
            // Novo cliente
            printf("Novo cliente: %s\n", content);

            pthread_mutex_lock(&manager->mutex);

            if (valida_user(manager, content))
            {
                printf("User %s já existe.\n", content);
                pthread_mutex_unlock(&manager->mutex);
                continue;
            }
            add_user(manager, content);

            char fifo_pessoal[FIFO_NAME_LENGHT];
            snprintf(fifo_pessoal, sizeof(fifo_pessoal), FIFO_PESSOAL, content);

            if (access(fifo_pessoal, F_OK) != 0)
            {
                if (mkfifo(fifo_pessoal, 0760) == -1)
                {
                    printf("Erro ao criar FIFO pessoal para %s.\n", content);
                    pthread_mutex_unlock(&manager->mutex);
                    continue;
                }
            }

            int fdr = open(fifo_pessoal, O_WRONLY);
            if (fdr == -1)
            {
                printf("Erro ao abrir FIFO pessoal %s para %s.\n", fifo_pessoal, content);
                pthread_mutex_unlock(&manager->mutex);
                continue;
            }

            const char *sucesso = "Entrou com sucesso!\n";
            if (write(fdr, sucesso, strlen(sucesso) + 1) == -1)
            {
                printf("Erro ao enviar mensagem de sucesso para %s.\n", content);
            }
            close(fdr);
            printf("Mensagem de sucesso enviada para %s.\n", content);

            pthread_mutex_unlock(&manager->mutex);
        }
        else if (msg_type == MESSAGE_TYPE_CMD)
        {
            // Comando de cliente
            printf("Mensagem recebida: %s\n", content);
            printf("Username: %s\n", username);

            if (username)
            {
                username[MAX_USER_NAME - 1] = '\0';
            }
            else
            {
                printf("Erro: não foi possível identificar o usuário no comando.\n");
                continue;
            }

            ClientData client_data = {
                .manager = manager,
            };
            strncpy(client_data.username, username, sizeof(client_data.username) - 1);
            snprintf(client_data.fifo_pessoal, sizeof(client_data.fifo_pessoal), FIFO_PESSOAL, username);

            command_handler(content, &client_data, username);
        }
    }

    pthread_exit(NULL);
}

int valida_user(Manager *manager, const char *username)
{

    printf("Validando o nome de usuário: %s\n", username);

    if (manager->user_count == MAX_USERS)
    {
        printf("Número de utilizadores máximo foi atingido!\n");
        pthread_mutex_unlock(&manager->mutex);
        return 1;
    }

    for (int i = 0; i < manager->user_count; i++)
    {
        if (strcmp(manager->users[i].username, username) == 0)
        {
            printf("Esse nome já existe!\n");
            pthread_mutex_unlock(&manager->mutex);
            return 1;
        }
    }

    return 0;
    // a funcionar
}

void add_user(Manager *manager, const char *username)
{
    if (manager->user_count < MAX_USERS)
    {
        strncpy(manager->users[manager->user_count].username, username, sizeof(manager->users[manager->user_count].username) - 1);
        manager->users[manager->user_count].username[sizeof(manager->users[manager->user_count].username) - 1] = '\0'; // Garantir terminação '\0'
        manager->users[manager->user_count].is_connected = 1;
        manager->user_count++;
    }
    // a funcionar
}

void command_handler(const char *command, ClientData *client_data, const char *username)
{
    char *args[10];
    int num_parts = 0;

    printf("A processar comando: %s\n", command);

    // Tokenizar o comando recebido
    char buffer[256];
    strncpy(buffer, command, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    char fifo_pessoal[FIFO_NAME_LENGHT];
    strncpy(fifo_pessoal, client_data->fifo_pessoal, sizeof(client_data->fifo_pessoal));
    printf("[COMMAND HANDLER]FIFO pessoal: %s\n", fifo_pessoal);

    char *token = strtok(buffer, " ");
    while (token != NULL && num_parts < 10)
    {
        args[num_parts++] = token;
        token = strtok(NULL, " ");
    }

    if (num_parts > 0)
    {
        if (strcmp(args[0], "topics") == 0)
        {

            // Exibir tópicos
            if (show_topics(client_data->manager) == 0)
            {
                // Montar string com todos os tópicos
                char topic_list[512] = "Tópicos disponíveis:\n";
                for (int i = 0; i < client_data->manager->topic_count; i++)
                {
                    strcat(topic_list, client_data->manager->topics[i].name);
                    strcat(topic_list, "\n");
                }

                // Abrir FIFO pessoal para escrita
                int fdr = open(fifo_pessoal, O_WRONLY);
                if (fdr == -1)
                {
                    printf("Erro ao abrir FIFO pessoal para enviar tópicos");
                    close(fdr);
                    return;
                }

                // Enviar lista de tópicos
                if (write(fdr, topic_list, strlen(topic_list) + 1) == -1)
                {
                    printf("Erro ao enviar tópicos pelo FIFO pessoal");
                    close(fdr);
                    return;
                }
                else
                {
                    printf("Tópicos enviados ao cliente %s pelo FIFO pessoal.\n", username);
                }
                close(fdr);
            }
            else if (show_topics(client_data->manager) == 1)
            {
                // Caso não haja tópicos, enviar mensagem ao cliente
                char no_topics_msg[] = "Não existem tópicos disponíveis.\n";

                int fdr = open(fifo_pessoal, O_WRONLY);
                if (fdr == -1)
                {
                    printf("Erro ao abrir FIFO pessoal para enviar mensagem de erro");
                    close(fdr);
                    return;
                }

                if (write(fdr, no_topics_msg, strlen(no_topics_msg) + 1) == -1)
                {
                    printf("Erro ao enviar mensagem de erro pelo FIFO pessoal");
                    close(fdr);
                    return;
                }
                else
                {
                    printf("Escrevi %s no %s\n", no_topics_msg, fifo_pessoal);
                    close(fdr);
                }
            }
            else
            {
                printf("Erro ao mostrar os tópicos\n");
            }
        }

        else if (strcmp(args[0], "msg") == 0)
        {
            printf("Cheguei a esta parte!\n");
            if (num_parts >= 4)
            {
                Message *message = malloc(sizeof(Message));
                // copiar para message-> autor o client_data->username;
                strncpy(message->autor.username, client_data->username, sizeof(client_data->username) - 1);
                printf("O user é: %s\n", message->autor.username);
                strncpy(message->topic, args[1], sizeof(args[1]) - 1);
                printf("O tópico é: %s\n", message->topic);
                message->expiration_time = atoi(args[2]);
                if (message->expiration_time < 0)
                {
                    printf("Duração inválida\n");
                    free(message);
                    return;
                }
                printf("O duração é: %d\n", message->expiration_time);
                strncpy(message->message, args[3], sizeof(args[3]) - 1);
                printf("A mensagem é: %s\n", message->message);

                printf("Verificar se o add_message funciona!\n");
                if (add_message_to_manager(client_data->manager, message) == 0)

                {
                    printf("Funciona\n");
                }
                else
                {
                    printf("Não funciona!\n");
                }

                send_message(message, client_data);
                free(message);
            }
            else
            {
                printf("Comando msg inválido. Formato esperado: msg <topico> <duracao> <msg>\n");
            }
        }
        else if (strcmp(args[0], "subscribe") == 0)
        {
            if (num_parts >= 2)
            {
                if (subscribe_to_topic(args[1], client_data) == 0)
                {
                    char buffer[] = "Subscrição efetuada com sucesso\n";
                    printf("Subscrição efetuada com sucesso\n");
                    int fdr = open(fifo_pessoal, O_WRONLY);
                    if (fdr == -1)
                    {
                        printf("Erro ao abrir FIFO pessoal para enviar mensagem de sucesso");
                        close(fdr);
                        return;
                    }

                    if (write(fdr, buffer, strlen(buffer) + 1) == -1)
                    {
                        printf("Erro ao enviar mensagem de sucesso pelo FIFO pessoal");
                        close(fdr);
                        return;
                    }
                    else
                    {
                        printf("Escrevi %s no %s\n", buffer, fifo_pessoal);
                        close(fdr);
                    }
                }
                else
                {
                    char buffer[] = "Erro ao subscrever o tópico\n";
                    printf("Erro ao subscrever o tópico\n");
                    int fdr = open(fifo_pessoal, O_WRONLY);
                    if (fdr == -1)
                    {
                        printf("Erro ao abrir FIFO pessoal para enviar mensagem de erro");
                        close(fdr);
                        return;
                    }

                    if (write(fdr, buffer, strlen(buffer) + 1) == -1)
                    {
                        printf("Erro ao enviar mensagem de erro pelo FIFO pessoal");
                        close(fdr);
                        return;
                    }
                    else
                    {
                        printf("Escrevi %s no %s\n", buffer, fifo_pessoal);
                        close(fdr);
                    }
                }
            }
            else
            {
                char invalido[] = "Comando subscribe inválido. Formato esperado: subscribe <topico>\n";
                int fdr = open(fifo_pessoal, O_WRONLY);
                if (fdr == -1)
                {
                    printf("Erro ao abrir FIFO pessoal para enviar mensagem de comando inválido\n");
                    close(fdr);
                    return;
                }

                if (write(fdr, invalido, strlen(invalido) + 1) == -1)
                {
                    printf("Erro ao enviar mensagem de comando inválido pelo FIFO pessoal");
                    close(fdr);
                    return;
                }
                else
                {
                    printf("Escrevi %s no %s\n", invalido, fifo_pessoal);
                    close(fdr);
                }
            }
        }
        else if (strcmp(args[0], "unsubscribe") == 0)
        {
            if (num_parts >= 2)
            {
                if (unsubscribe_from_topic(args[1], client_data->manager) == 0)
                {
                    char buffer[] = "Retirou a subscrição com sucesso\n";
                    printf("Retirou a subscrição com sucesso\n");
                    int fdr = open(fifo_pessoal, O_WRONLY);
                    if (fdr == -1)
                    {
                        printf("Erro ao abrir FIFO pessoal para enviar mensagem de sucesso");
                        close(fdr);
                        return;
                    }

                    if (write(fdr, buffer, strlen(buffer) + 1) == -1)
                    {
                        printf("Erro ao enviar mensagem de sucesso pelo FIFO pessoal");
                        close(fdr);
                        return;
                    }
                    else
                    {
                        printf("Escrevi %s no %s\n", buffer, fifo_pessoal);
                        close(fdr);
                    }
                }
                else
                {
                    char buffer[] = "Erro ao retirar a subscrição do tópico\n";
                    printf("Erro ao retirar a subscrição do tópico\n");
                    int fdr = open(fifo_pessoal, O_WRONLY);
                    if (fdr == -1)
                    {
                        printf("Erro ao abrir FIFO pessoal para enviar mensagem de erro");
                        close(fdr);
                        return;
                    }

                    if (write(fdr, buffer, strlen(buffer) + 1) == -1)
                    {
                        printf("Erro ao enviar mensagem de erro pelo FIFO pessoal");
                        close(fdr);
                        return;
                    }
                    else
                    {
                        printf("Escrevi %s no %s\n", buffer, fifo_pessoal);
                        close(fdr);
                    }
                }
            }
            else
            {
                char invalido[] = "Comando unsubscribe inválido. Formato esperado: unsubscribe <topico>\n";
                int fdr = open(fifo_pessoal, O_WRONLY);
                if (fdr == -1)
                {
                    printf("Erro ao abrir FIFO pessoal para enviar mensagem de comando inválido\n");
                    close(fdr);
                    return;
                }

                if (write(fdr, invalido, strlen(invalido) + 1) == -1)
                {
                    printf("Erro ao enviar mensagem de comando inválido pelo FIFO pessoal");
                    close(fdr);
                    return;
                }
                else
                {
                    printf("Escrevi %s no %s\n", invalido, fifo_pessoal);
                    close(fdr);
                }
            }
        }
        else if (strcmp(args[0], "exit") == 0)
        {
            printf("A retirar o user %s do sistema.\n", username);
            if (remove_user(client_data->manager, username) == 0)
            {
                char buffer[] = "Foi removido do sistema\n";
                int fdr = open(fifo_pessoal, O_WRONLY);
                if (fdr == -1)
                {
                    printf("Erro ao abrir FIFO pessoal para enviar mensagem de término");
                    close(fdr);
                    return;
                }

                if (write(fdr, buffer, strlen(buffer) + 1) == -1)
                {
                    printf("Erro ao enviar mensagem de término pelo FIFO pessoal");
                    close(fdr);
                    return;
                }
                else
                {
                    printf("Escrevi %s no %s\n", buffer, fifo_pessoal);
                    close(fdr);
                }
            }
            else
            {
                printf("Erro ao remover o user %s do sistema.\n", username);
                char buffer[] = "Não consegui remover do sistema!\n";

                int fdr = open(fifo_pessoal, O_WRONLY);
                if (fdr == -1)
                {
                    printf("Erro ao abrir FIFO pessoal para enviar mensagem de término");
                    close(fdr);
                    return;
                }

                if (write(fdr, buffer, strlen(buffer) + 1) == -1)
                {
                    printf("Erro ao enviar mensagem de término pelo FIFO pessoal");
                    close(fdr);
                    return;
                }
                else
                {
                    printf("Escrevi %s no %s\n", buffer, fifo_pessoal);
                    close(fdr);
                }
            }
        }
        else
        {
            printf("Comando não reconhecido: %s\n", args[0]);
        }
    }
    else
    {
        printf("Comando vazio recebido.\n");
    }
}

int remove_user(Manager *manager, const char *username)
{
    for (int i = 0; i < manager->user_count; i++)
    {
        if (strcmp(manager->users[i].username, username) == 0)
        {
            // Organizar o array
            for (int j = i; j < manager->user_count - 1; j++)
            {
                manager->users[j] = manager->users[j + 1];
            }
            manager->user_count--;
            printf("Usuário %s removido.\n", username);
            return 0;
        }
    }
    printf("Usuário %s não encontrado para remoção.\n", username);
    return -1;
}

int subscribe_to_topic(const char *topic_name, ClientData *clientData)
{
    printf("A subscrever o tópico %s para o user %s\n", topic_name, clientData->username);

    for (int i = 0; i < clientData->manager->user_count; i++)
    {
        if (strcmp(clientData->manager->users[i].username, clientData->username) == 0)
        {
            printf("Usuário %s encontrado.\n", clientData->username);

            for (int j = 0; j < clientData->manager->users[i].subscription_count; j++)
            {
                if (strcmp(clientData->manager->users[i].subscribed_topics[j], topic_name) == 0)
                {
                    printf("User %s já está inscrito no tópico %s\n", clientData->username, topic_name);
                    return -1; // Usuário já inscrito no tópico
                }
            }

            // Adicionar tópico à lista de subscrições do usuário
            if (clientData->manager->users[i].subscription_count < MAX_TOPICS)
            {
                strncpy(clientData->manager->users[i].subscribed_topics[clientData->manager->users[i].subscription_count], topic_name, MAX_TOPIC_NAME - 1);
                clientData->manager->users[i].subscribed_topics[clientData->manager->users[i].subscription_count][MAX_TOPIC_NAME - 1] = '\0'; // Garantir terminação nula
                clientData->manager->users[i].subscription_count++;
                printf("O user %s subscreveu o tópico %s\n", clientData->username, topic_name);
                return 0;
            }
            else
            {
                printf("O usuário já está inscrito no número máximo de tópicos.\n");
                return -2;
            }
        }
    }

    printf("Usuário %s não encontrado.\n", clientData->username);
    return -3; // Usuário não encontrado
}

int unsubscribe_from_topic(const char *topic_name, Manager *manager)
{
    printf("A cancelar a subscrição do tópico %s para o user %s\n", topic_name, manager->users->username);

    for (int i = 0; i < manager->user_count; i++)
    {
        if (strcmp(manager->users[i].username, manager->users->username) == 0)
        {
            printf("Usuário %s encontrado.\n", manager->users->username);

            for (int j = 0; j < manager->users[i].subscription_count; j++)
            {
                if (strcmp(manager->users[i].subscribed_topics[j], topic_name) == 0)
                {
                    printf("Tópico %s encontrado na lista de subscrições.\n", topic_name);

                    for (int k = j; k < manager->users[i].subscription_count - 1; k++)
                    {
                        strncpy(
                            manager->users[i].subscribed_topics[k],
                            manager->users[i].subscribed_topics[k + 1],
                            MAX_TOPIC_NAME);
                        manager->users[i].subscribed_topics[k][MAX_TOPIC_NAME - 1] = '\0'; // Garantir terminação nula
                    }

                    // Limpar a última posição após reorganizar
                    memset(manager->users[i].subscribed_topics[manager->users[i].subscription_count - 1], 0, MAX_TOPIC_NAME);

                    manager->users[i].subscription_count--;
                    printf("Tópico %s removido com sucesso para o usuário %s.\n", topic_name, manager->users->username);
                    return 0;
                }
            }

            printf("Usuário %s não está inscrito no tópico %s.\n", manager->users->username, topic_name);
            return -1;
        }
    }

    printf("Usuário %s não encontrado.\n", manager->users->username);
    return -2;
}

int send_message(Message *message, ClientData *clientData)
{
    printf("A enviar mensagem do user '%s' para o tópico '%s', durante %d segundos\n",
           message->autor.username, message->topic, message->expiration_time);

    // Verificações iniciais
    if (clientData == NULL || clientData->manager == NULL)
    {
        printf("Erro: clientData ou manager não inicializado.\n");
        return -1;
    }

    if (clientData->manager->topics == NULL)
    {
        printf("Erro: Lista de tópicos não inicializada.\n");
        return -1;
    }

    if (clientData->manager->topic_count <= 0)
    {
        printf("Erro: Nenhum tópico disponível.\n");
        return -1;
    }

    // Verificar se o tópico existe
    printf("Número de tópicos: %d\n", clientData->manager->topic_count);
    Topic *target_topic = NULL;

    for (int i = 0; i < clientData->manager->topic_count; i++)
    {
        printf("Comparando '%s' com tópico[%d]: '%s'\n", message->topic, i, clientData->manager->topics[i].name);
        if (strcmp(clientData->manager->topics[i].name, message->topic) == 0)
        {
            target_topic = &clientData->manager->topics[i];
            break;
        }
    }

    printf("Passei o primeiro ciclo\n");

    if (target_topic == NULL)
    {
        printf("Erro: Tópico '%s' não encontrado.\n", message->topic);
        return -1;
    }
    printf("Passei o target_topic\n");

    // Persistência e inicialização do temporizador
    if (message->expiration_time > 0)
    {
        pthread_mutex_lock(&clientData->manager->mutex);

        if (target_topic->messages == NULL)
        {
            printf("Erro: Array de mensagens não inicializado no tópico '%s'.\n", target_topic->name);
            pthread_mutex_unlock(&clientData->manager->mutex);
            return -1;
        }

        if (target_topic->message_count < MAX_PERSISTENT)
        {
            target_topic->messages[target_topic->message_count++] = *message;
            printf("Mensagem armazenada no tópico '%s'.\n", target_topic->name);

            TimerThreadArgs *args = malloc(sizeof(TimerThreadArgs));
            if (args == NULL)
            {
                printf("Erro: Falha ao alocar memória para TimerThreadArgs.\n");
                pthread_mutex_unlock(&clientData->manager->mutex);
                return -3;
            }
            args->manager = clientData->manager;
            args->topic_index = target_topic - clientData->manager->topics;
            args->message_index = target_topic->message_count - 1;

            pthread_t timer_thread;
            pthread_create(&timer_thread, NULL, start_message_timer_thread, args);
            pthread_detach(timer_thread);
        }
        else
        {
            printf("Erro: Tópico '%s' atingiu o limite máximo de mensagens persistentes.\n", target_topic->name);
            pthread_mutex_unlock(&clientData->manager->mutex);
            return -2;
        }
        pthread_mutex_unlock(&clientData->manager->mutex);
    }

    // Distribuir a mensagem para os assinantes
    distribute_message_to_subscribers(message, clientData->manager);

    return 0; // Sucesso
}

int show_topics(Manager *manager)
{
    // Simular tópicos disponíveis
    manager->topic_count = 2;
    strncpy(manager->topics[0].name, "topic1", sizeof(manager->topics[0].name));
    strncpy(manager->topics[1].name, "topic2", sizeof(manager->topics[1].name));

    if (manager->topic_count == 0)
    {
        printf("Não existem tópicos disponíveis.\n");
        return 1;
    }
    else
    {
        printf("Tópicos disponíveis:\n");
        for (int i = 0; i < manager->topic_count; i++)
        {
            printf("%s\n", manager->topics[i].name);
        }
        return 0;
    }
}

int add_message_to_manager(Manager *manager, Message *message)
{
    // Trava o mutex para evitar condições de corrida
    pthread_mutex_lock(&manager->mutex);

    // Verifica se há espaço para mais tópicos
    if (manager->topic_count >= MAX_TOPICS)
    {
        printf("Manager cheio, não pode armazenar mais mensagens!\n");
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }

    // Procura pelo tópico correspondente no manager
    for (int i = 0; i < manager->topic_count; i++)
    {
        if (strcmp(manager->topics[i].name, message->topic) == 0)
        {
            Topic *topic = &manager->topics[i];

            // Verifica se o tópico tem espaço para mais mensagens
            if (topic->message_count >= MAX_PERSISTENT)
            {
                printf("Tópico '%s' está cheio, não pode armazenar mais mensagens!\n", message->topic);
                pthread_mutex_unlock(&manager->mutex);
                return -2;
            }

            // Adiciona a mensagem ao tópico
            topic->messages[topic->message_count] = *message;
            topic->message_count++;
            printf("Mensagem adicionada ao tópico '%s'. Total de mensagens: %d\n",
                   topic->name, topic->message_count);

            pthread_mutex_unlock(&manager->mutex);
            return 0; // Sucesso
        }
    }

    // Caso o tópico não exista, cria um novo
    Topic *new_topic = &manager->topics[manager->topic_count];
    strncpy(new_topic->name, message->topic, MAX_TOPIC_NAME);
    new_topic->messages[0] = *message;
    new_topic->message_count = 1;
    new_topic->state = TOPIC_UNLOCKED; // Inicializa como desbloqueado por padrão
    manager->topic_count++;

    printf("Novo tópico '%s' criado e mensagem adicionada. Total de tópicos: %d\n",
           new_topic->name, manager->topic_count);

    pthread_mutex_unlock(&manager->mutex);
    return 0; // Sucesso
}

void remove_expired_message(Manager *manager, int topic_index, int message_index)
{
    // Validar índices do tópico e da mensagem
    if (topic_index < 0 || topic_index >= manager->topic_count)
    {
        printf("Índice de tópico inválido ao remover mensagem.\n");
        return;
    }

    Topic *topic = &manager->topics[topic_index];

    if (message_index < 0 || message_index >= topic->message_count)
    {
        printf("Índice de mensagem inválido ao remover mensagem.\n");
        return;
    }

    // Liberar a mensagem do índice especificado
    // Aqui assumimos que `message` pode ser tratada como string estática.
    memset(&topic->messages[message_index], 0, sizeof(Message));

    // Reorganizar mensagens para preencher o espaço vazio
    for (int i = message_index; i < topic->message_count - 1; i++)
    {
        topic->messages[i] = topic->messages[i + 1];
    }

    // Atualizar a contagem de mensagens
    topic->message_count--;

    printf("Mensagem removida. Total de mensagens restantes no tópico '%s': %d\n",
           topic->name, topic->message_count);
}

void *start_message_timer_thread(void *args)
{
    TimerThreadArgs *thread_args = (TimerThreadArgs *)args;
    Manager *manager = thread_args->manager;
    int topic_index = thread_args->topic_index;
    int message_index = thread_args->message_index;

    Topic *topic = &manager->topics[topic_index];
    Message *message = &topic->messages[message_index];

    printf("Thread iniciada para temporizador da mensagem no tópico '%s' por %d segundos.\n",
           topic->name, message->expiration_time);

    sleep(message->expiration_time);

    // Após a expiração, remover a mensagem
    pthread_mutex_lock(&manager->mutex);
    if (message_index < topic->message_count &&
        memcmp(&topic->messages[message_index], message, sizeof(Message)) == 0)
    {
        printf("Mensagem expirada. Removendo mensagem do tópico '%s'.\n", topic->name);
        remove_expired_message(manager, topic_index, message_index);
    }
    else
    {
        printf("Mensagem já removida ou modificada durante o temporizador.\n");
    }
    pthread_mutex_unlock(&manager->mutex);

    free(args); // Liberar memória alocada para os argumentos
    return NULL;
}

void distribute_message_to_subscribers(Message *message, Manager *manager)
{
    printf("[DEBUG] Iniciando distribuição da mensagem para o tópico: '%s'\n", message->topic);

    pthread_mutex_lock(&manager->mutex); // Proteger contra condições de corrida

    int total_users_notified = 0; // Contador para monitorar quantos usuários foram notificados

    // Iterar sobre todos os usuários no Manager
    for (int i = 0; i < manager->user_count; i++)
    {
        User *user = &manager->users[i];
        printf("[DEBUG] Verificando usuário: '%s'\n", user->username);

        // Verificar se o usuário está conectado
        if (!user->is_connected)
        {
            printf("[INFO] Usuário '%s' não está conectado. Pulando...\n", user->username);
            continue;
        }

        // Verificar se o usuário está inscrito no tópico
        int is_subscribed = 0;
        for (int j = 0; j < user->subscription_count; j++)
        {
            printf("[DEBUG] Verificando inscrição do usuário '%s' no tópico '%s'...\n",
                   user->username, user->subscribed_topics[j]);

            if (strcmp(user->subscribed_topics[j], message->topic) == 0)
            {
                is_subscribed = 1;
                printf("[INFO] Usuário '%s' está inscrito no tópico '%s'\n",
                       user->username, message->topic);
                break;
            }
        }

        if (is_subscribed)
        {
            // Construir o nome do FIFO pessoal do usuário
            char fifo_pessoal[FIFO_NAME_LENGHT];
            snprintf(fifo_pessoal, sizeof(fifo_pessoal), FIFO_PESSOAL, user->username);
            printf("[DEBUG] FIFO pessoal do usuário '%s': %s\n", user->username, fifo_pessoal);

            // Abrir o FIFO para escrita
            int fdr = open(fifo_pessoal, O_WRONLY);
            if (fdr == -1)
            {
                perror("[ERRO] Erro ao abrir FIFO pessoal");
                printf("[INFO] Não foi possível enviar a mensagem para o usuário '%s'\n", user->username);
                continue;
            }
            printf("[DEBUG] FIFO '%s' aberto para escrita com sucesso.\n", fifo_pessoal);

            // Construir a mensagem para o usuário
            char buffer[512];
            snprintf(buffer, sizeof(buffer), "%s:%s:%s\n",
                     message->topic,
                     message->autor.username,
                     message->message);

            // Escrever a mensagem no FIFO
            if (write(fdr, buffer, strlen(buffer)) == -1)
            {
                perror("[ERRO] Erro ao enviar mensagem pelo FIFO pessoal");
            }
            else
            {
                printf("[INFO] Mensagem enviada para o usuário '%s' via FIFO: '%s'\n", user->username, buffer);
                total_users_notified++;
            }

            close(fdr); // Fechar o descritor de arquivo após uso
        }
        else
        {
            printf("[INFO] Usuário '%s' não está inscrito no tópico '%s'. Pulando...\n",
                   user->username, message->topic);
        }
    }

    pthread_mutex_unlock(&manager->mutex); // Liberar o mutex

    printf("[INFO] Distribuição finalizada. Total de usuários notificados: %d\n", total_users_notified);
}

void save_messages_to_env(Manager *manager)
{
    FILE *file = fopen("/tmp/msg_fich.txt", "w");
    if (!file)
    {
        perror("Erro ao abrir arquivo para salvar mensagens persistentes");
        return;
    }

    // Percorrer todos os tópicos e salvar mensagens persistentes
    for (int i = 0; i < manager->topic_count; i++)
    {
        Topic *topic = &manager->topics[i];
        for (int j = 0; j < topic->message_count; j++)
        {
            Message *msg = &topic->messages[j];
            fprintf(file, "Tópico: %s, Autor: %s, Mensagem: %s, Tempo: %d\n",
                    topic->name, msg->autor.username, msg->message, msg->expiration_time);
        }
    }

    fclose(file);

    // Atualizar variável de ambiente com o caminho do arquivo
    setenv("MSG_FICH", "/tmp/msg_fich.txt", 1);
    printf("Mensagens persistentes salvas em MSG_FICH.\n");
}

void handle_signal(int sig)
{
    printf("Sinal %d recebido. Salvando mensagens...\n", sig);
    extern Manager *global_manager; // Manager global para salvar mensagens
    if (global_manager)
    {
        save_messages_to_env(global_manager);
    }
    exit(0);
}

int list_users(Manager *manager)
{
    pthread_mutex_lock(&manager->mutex);

    if (manager->user_count == 0)
    {
        printf("Nenhum utilizador registrado na plataforma.\n");
    }
    else
    {
        printf("Utilizadores na plataforma:\n");
        for (int i = 0; i < manager->user_count; i++)
        {
            printf(" - %s\n", manager->users[i].username);
        }
    }

    pthread_mutex_unlock(&manager->mutex);
    return 0;
}

int list_topic_messages(Manager *manager, const char *topic_name)
{
    pthread_mutex_lock(&manager->mutex);

    int topic_found = 0;

    for (int i = 0; i < manager->topic_count; i++)
    {
        if (strcmp(manager->topics[i].name, topic_name) == 0)
        {
            topic_found = 1;
            printf("Mensagens no tópico '%s':\n", topic_name);
            for (int j = 0; j < manager->topics[i].message_count; j++)
            {
                printf(" - %s\n", manager->topics[i].messages[j].message);
            }
            break;
        }
    }

    pthread_mutex_unlock(&manager->mutex);

    if (!topic_found)
    {
        printf("Tópico '%s' não encontrado.\n", topic_name);
        return -1;
    }

    return 0;
}

int lock_topic(Manager *manager, const char *topic_name)
{
    pthread_mutex_lock(&manager->mutex);

    int topic_found = 0;

    for (int i = 0; i < manager->topic_count; i++)
    {
        if (strcmp(manager->topics[i].name, topic_name) == 0)
        {
            topic_found = 1;
            manager->topics[i].state = TOPIC_LOCKED;
            printf("Tópico '%s' bloqueado.\n", topic_name);
            break;
        }
    }

    pthread_mutex_unlock(&manager->mutex);

    if (!topic_found)
    {
        printf("Tópico '%s' não encontrado.\n", topic_name);
        return -1;
    }

    return 0;
}

int unlock_topic(Manager *manager, const char *topic_name)
{
    pthread_mutex_lock(&manager->mutex);

    int topic_found = 0;

    for (int i = 0; i < manager->topic_count; i++)
    {
        if (strcmp(manager->topics[i].name, topic_name) == 0)
        {
            topic_found = 1;
            manager->topics[i].state = TOPIC_UNLOCKED;
            printf("Tópico '%s' desbloqueado.\n", topic_name);
            break;
        }
    }

    pthread_mutex_unlock(&manager->mutex);

    if (!topic_found)
    {
        printf("Tópico '%s' não encontrado.\n", topic_name);
        return -1;
    }

    return 0;
}

int main()
{

    Manager manager = {.user_count = 0};
    global_manager = &manager;

    pthread_mutex_init(&manager.mutex, NULL);

    createFifoGeral();

    pthread_t request_tid;
    pthread_create(&request_tid, NULL, clientHandler, &manager);

    printf("Manager ativo. Aguardando conexões...\n");

    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    char input[120];
    char buffer[100];
    char cmd[20];

    while (1)
    {
        printf("Insira um comando: \n\n");
        printf("users - ver os utilizadores\n");
        printf("remove <user> - remover o utilizador\n");
        printf("topics - listar os topicos\n");
        printf("show <topic> - ver os topicos\n");
        printf("lock <topic> - bloquear um topico\n");
        printf("unlock <topic> - desbloquear um topico\n");
        printf("close - fechar o manager\n");
        // Lê uma linha inteira com segurança
        fgets(input, sizeof(buffer), stdin);
        buffer[strcspn(input, "\n")] = '\0';
        sscanf(input, "%s %[^\n]", cmd, buffer);

        printf("Você digitou: %s : %s\n", cmd, buffer);

        if (strcmp(cmd, "users") == 0)
        {
            list_users(&manager);
        }
        else if (strcmp(cmd, "remove") == 0)
        {
            remove_user(&manager, buffer);
        }
        else if (strcmp(cmd, "topics") == 0)
        {
            // mostra os topicos e numero de persistentes
            show_topics(&manager);
        }
        else if (strcmp(cmd, "show") == 0)
        {
            list_topic_messages(&manager, buffer);
        }
        else if (strcmp(cmd, "lock") == 0)
        {
            lock_topic(&manager, buffer);
        }
        else if (strcmp(cmd, "unlock") == 0)
        {
            unlock_topic(&manager, buffer);
        }

        else if (strcmp(cmd, "close") == 0)
        {
            pthread_join(request_tid, NULL);
            pthread_mutex_destroy(&manager.mutex);
            unlink(FIFO_GERAL);
            return 1;
        }
        else
        {
            printf("Comando não reconhecido!\n");
        }
    }

    // Esperar pelo término do request handler
    pthread_join(request_tid, NULL);

    pthread_mutex_destroy(&manager.mutex);
    unlink(FIFO_GERAL);

    return 0;
}
