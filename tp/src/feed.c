#include "feed.h"

void verify_manager()
{
    if (access(FIFO_GERAL, F_OK) != 0)
    {
        printf("O manager deve ser iniciado antes do feed!\n");
        exit(1);
        // a funcionar
    }
}

void init_user(User *user, const char *username)
{
    strncpy(user->username, username, sizeof(user->username) - 1);
    user->username[sizeof(user->username) - 1] = '\0';

    user->id = getpid();
    printf("O seu ID é: %d \n", user->id);
    printf("O seu nome é: %s\n", user->username);
}

int send_info(User *user)
{

    int fdr = open(FIFO_GERAL, O_WRONLY);
    if (fdr == -1)
    {
        perror("Erro ao abrir o fifo geral");
        return -2;
    }

    if (strlen(user->username) == 0)
    {
        printf("Nome de usuário inválido!\n");
        close(fdr);
        return -3;
    }

    char msg[100];
    snprintf(msg, sizeof(msg), "%d:%s", MESSAGE_TYPE_CONN, user->username);
    printf("A enviar informações ao servidor: %s\n", user->username);
    if (write(fdr, msg, strlen(msg) + 1) == -1)
    {
        perror("Erro ao escrever no fifo");
        close(fdr);
        return -4;
    }

    printf("Informação enviada com sucesso!\n");

    close(fdr);
}

int receive_log_status(User *user)
{
    char response[50];
    char fifo_pessoal[FIFO_NAME_LENGHT];
    snprintf(fifo_pessoal, sizeof(fifo_pessoal), "fifo_%s", user->username);
    printf("Já personalizei o fifo pessoal: %s\n", fifo_pessoal);

    sleep(1); // dorme 1 segundo
    int fdl = open(fifo_pessoal, O_RDWR);
    if (fdl == -1)
    {
        printf("Erro ao abrir %s para leitura!\n", fifo_pessoal);
        return -5;
    }
    printf("Aguardando resposta do servidor no fifo_pessoal...\n");

    // ler dados do fifo pessoal
    ssize_t nbytes = read(fdl, response, sizeof(response) - 1);
    if (nbytes < 0)
    {
        printf("Erro ao ler a mensagem do servidor no %s!\n", fifo_pessoal);
        close(fdl);
        return -6;
    }
    response[nbytes] = '\0';
    printf("Recebeu a seguinte mensagem do servidor: %s\n", response);

    // Verificando se a resposta é a esperada
    if (strcmp(response, "Entrou com sucesso!\n") != 0)
    {
        printf("Teve o acesso negado! A encerrar!\n");
        close(fdl);
        return -7;
    }

    // Fechar o FIFO geral após a leitura
    close(fdl);

    return 1;
}

void *sender_thread(void *arg)
{
    Feed *feed = (Feed *)arg;
    char input[256];

    do
    {
        printf("Escolha uma opção:\n");
        printf("[topics] - Listar todos os tópicos\n");
        printf("[msg <topico> <duracao> <msg>] - Enviar mensagem para um tópico\n");
        printf("[subscribe <topico>] - Subscrever um tópico\n");
        printf("[unsubscribe <topico>] - Tirar a subscrição de um tópico\n");
        printf("[exit] - Sair da aplicação\n");

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            printf("Erro ao ler entrada!\n");
            continue;
        }

        input[strcspn(input, "\n")] = '\0';

        int fdr = open(FIFO_GERAL, O_WRONLY);
        if (fdr == -1)
        {
            printf("Erro ao abrir o FIFO para escrita!\n");
            exit(3);
        }

        char msg[256];
        snprintf(msg, sizeof(msg), "%d:%s:%s", MESSAGE_TYPE_CMD, input, feed->user.username);
        printf("A enviar informações ao servidor: %s\n", msg);

        if (write(fdr, msg, strlen(msg) + 1) == -1)
        {
            printf("Erro ao enviar comando para o manager");
            exit(4);
        }

        close(fdr);

    }while (strcmp(input, "exit") != 0);

    pthread_exit(NULL);

    return NULL;
}

void *receiver_thread(void *arg)
{
    Feed *feed = (Feed *)arg;
    char buffer[512];

    while (feed->running)
    {
        int fdl = open(feed->fifo_pessoal, O_RDWR);
        if (fdl == -1)
        {
            printf("Erro ao abrir FIFO pessoal para leitura");
            exit(5);
        }

        ssize_t nbytes = read(fdl, buffer, sizeof(buffer) - 1);
        if (nbytes > 0)
        {
            buffer[nbytes] = '\0';
            printf("Resposta recebida do manager: %s\n", buffer);
            if(strcmp (buffer, "Foi removido do sistema\n") == 0)
            {
                feed->running = false;
            }
        }
        else if (nbytes == 0)
        {
            printf("FIFO pessoal fechado pelo manager.\n");
            feed->running = false;
        }
        else
        {
            printf("Erro ao ler do FIFO pessoal");
            exit(6);
        }

        close(fdl);
    }
    pthread_exit(NULL);

    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Número de parâmetros inválido!\n");
        return 1;
    }

    if (strlen(argv[1] + 1) > MAX_USER_NAME)
    {
        printf("Nome de utilizador demasiado comprido!\n");
        return 2;
    }

    verify_manager();

    User user;
    init_user(&user, argv[1]);

    if (send_info(&user) < 0)
    {
        printf("Ocorreu um erro a enviar a informação ao servidor\n");
        return 3;
    }
    printf("Enviei a informação ao servidor\n");

    if (receive_log_status(&user) != 1)
    {
        printf("Ocorreu um erro a registar-se!\n");
        return 4;
    }
    printf("Registado com sucesso!\n");

    Feed feed;
    feed.user = user;
    snprintf(feed.fifo_pessoal, sizeof(feed.fifo_pessoal), "fifo_%s", user.username);
    feed.running = true;

    pthread_t sender, receiver;

    // Criar threads
    if (pthread_create(&receiver, NULL, receiver_thread, &feed) != 0)
    {
        perror("Erro ao criar thread de recepção");
        return 1;
    }

    if (pthread_create(&sender, NULL, sender_thread, &feed) != 0)
    {
        perror("Erro ao criar thread de envio");
        feed.running = false;
        pthread_join(receiver, NULL);
        return 1;
    }

    // Aguardar threads
    pthread_join(sender, NULL);
    printf("Fechei o sender\n");
    feed.running = false; // Garantir que a recepção pare após o envio
    pthread_join(receiver, NULL);
    printf("Fechei o receiver\n");

    // Remover FIFO pessoal
    unlink(feed.fifo_pessoal);
    printf("FIFO pessoal removido.\n");

    return 0;
}
