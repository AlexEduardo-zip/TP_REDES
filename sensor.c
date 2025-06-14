#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>
#include "common.h"

int main(int argc, char *argv[])
{
    // Validação dos argumentos
    if (argc < 4)
    {
        fprintf(stderr, "Uso: %s <ip_servidor> <porta_sl> <porta_ss>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *ip_servidor = argv[1];
    int porta_sl = atoi(argv[2]);
    int porta_ss = atoi(argv[3]);

    // Configuração inicial do sensor
    srand(time(NULL));
    int id_localizacao = (rand() % 11) + 1;
    if (id_localizacao == 11)
        id_localizacao = -1;

    EstadoSensor estado = {0};

    printf("Sensor iniciando na localização %d...\n", id_localizacao);

    // Conexão com os servidores
    estado.socket_sl = conectar_servidor(ip_servidor, porta_sl);
    if (estado.socket_sl == SOCKET_ERROR)
        perror("Falha ao conectar ao SL");

    estado.socket_ss = conectar_servidor(ip_servidor, porta_ss);
    if (estado.socket_ss == SOCKET_ERROR)
    {
        close(estado.socket_sl);
        perror("Falha ao conectar ao SS");
    }

    // Registro nos servidores
    char id_localizacao_str[4];
    snprintf(id_localizacao_str, sizeof(id_localizacao_str), "%d", id_localizacao);
    construir_mensagem(estado.buffer_envio, sizeof(estado.buffer_envio),
                       REQ_CONNSEN, id_localizacao_str, NULL);

    printf("[SENSOR->SL] Enviando REQ_CONNSEN: \"%s\"\n", estado.buffer_envio);
    if (send(estado.socket_sl, estado.buffer_envio, strlen(estado.buffer_envio), 0) < 0)
        perror("envio para SL");

    printf("[SENSOR->SS] Enviando REQ_CONNSEN: \"%s\"\n", estado.buffer_envio);
    if (send(estado.socket_ss, estado.buffer_envio, strlen(estado.buffer_envio), 0) < 0)
        perror("envio para SS");

    // Configuração para multiplexação de I/O
    fd_set conjunto_principal, conjunto_leitura;
    int fd_max = (estado.socket_sl > estado.socket_ss) ? estado.socket_sl : estado.socket_ss;

    FD_ZERO(&conjunto_principal);
    FD_SET(estado.socket_sl, &conjunto_principal);
    FD_SET(estado.socket_ss, &conjunto_principal);
    FD_SET(STDIN_FILENO, &conjunto_principal);

    // Loop principal do sensor
    while (estado.socket_sl != -1 || estado.socket_ss != -1)
    {
        conjunto_leitura = conjunto_principal;
        if (estado.registrado_sl && estado.registrado_ss)
        {
            printf("\n> ");
            fflush(stdout);
        }

        if (select(fd_max + 1, &conjunto_leitura, NULL, NULL, NULL) < 0)
            perror("erro no select");

        for (int i = 0; i <= fd_max; i++)
        {
            if (FD_ISSET(i, &conjunto_leitura))
            {
                if (i == STDIN_FILENO)
                {
                    if (estado.registrado_sl && estado.registrado_ss)
                    {
                        char buffer_comando[MAX_MSG_SIZE];

                        // Lê entrada do usuário
                        if (fgets(buffer_comando, sizeof(buffer_comando), stdin) == NULL)
                        {
                            printf("\n[SENSOR] EOF detectado. Encerrando...\n");
                            construir_mensagem(estado.buffer_envio, sizeof(estado.buffer_envio),
                                               REQ_DISCSEN, estado.id_sensor_sl, NULL);
                            send(estado.socket_sl, estado.buffer_envio, strlen(estado.buffer_envio), 0);
                            construir_mensagem(estado.buffer_envio, sizeof(estado.buffer_envio),
                                               REQ_DISCSEN, estado.id_sensor_ss, NULL);
                            send(estado.socket_ss, estado.buffer_envio, strlen(estado.buffer_envio), 0);
                            // return;
                        }
                        else
                        {
                            // Remove newline
                            buffer_comando[strcspn(buffer_comando, "\n")] = '\0';

                            // Faz cópia para tokenização segura
                            char copia_temp[MAX_MSG_SIZE];
                            strncpy(copia_temp, buffer_comando, sizeof(copia_temp));
                            char *comando = strtok(copia_temp, " ");

                            if (comando)
                            {

                                if (strcmp(comando, "kill") == 0)
                                {
                                    printf("[SENSOR] Encerrando conexões...\n");
                                    construir_mensagem(estado.buffer_envio, sizeof(estado.buffer_envio),
                                                       REQ_DISCSEN, estado.id_sensor_sl, NULL);
                                    send(estado.socket_sl, estado.buffer_envio, strlen(estado.buffer_envio), 0);

                                    construir_mensagem(estado.buffer_envio, sizeof(estado.buffer_envio),
                                                       REQ_DISCSEN, estado.id_sensor_ss, NULL);
                                    send(estado.socket_ss, estado.buffer_envio, strlen(estado.buffer_envio), 0);
                                }
                                else if (strcmp(comando, "check") == 0)
                                {
                                    char *arg = strtok(NULL, "");
                                    if (arg && strcmp(arg, "failure") == 0)
                                    {
                                        printf("[SENSOR] Sending REQ_SENSSTATUS %s\n", estado.id_sensor_ss);
                                        construir_mensagem(estado.buffer_envio, sizeof(estado.buffer_envio),
                                                           REQ_SENSSTATUS, estado.id_sensor_ss, NULL);
                                        send(estado.socket_ss, estado.buffer_envio, strlen(estado.buffer_envio), 0);
                                    }
                                }
                                else if (strcmp(comando, "locate") == 0)
                                {
                                    char *id_alvo = strtok(NULL, " ");
                                    if (id_alvo)
                                    {
                                        printf("[SENSOR] Localizando sensor %s...\n", id_alvo);
                                        construir_mensagem(estado.buffer_envio, sizeof(estado.buffer_envio),
                                                           REQ_SENSLOC, id_alvo, NULL);
                                        send(estado.socket_sl, estado.buffer_envio, strlen(estado.buffer_envio), 0);
                                    }
                                }
                                else if (strcmp(comando, "diagnose") == 0)
                                {
                                    char *localizacao = strtok(NULL, " ");
                                    if (localizacao)
                                    {
                                        printf("[SENSOR] Diagnosticando localização %s...\n", localizacao);
                                        construir_mensagem(estado.buffer_envio, sizeof(estado.buffer_envio),
                                                           REQ_LOCLIST, estado.id_sensor_sl, localizacao);
                                        send(estado.socket_sl, estado.buffer_envio, strlen(estado.buffer_envio), 0);
                                    }
                                }
                                else
                                {
                                    printf("[SENSOR] Comando desconhecido: '%s'\n", comando);
                                }
                            }
                        }
                    }
                    else
                    {
                        char buffer_temp[1024];
                        fgets(buffer_temp, sizeof(buffer_temp), stdin);
                        printf("[SENSOR] Aguarde a conclusão do registro.\n");
                    }
                }
                else if (i == estado.socket_sl || i == estado.socket_ss)
                {
                    processar_mensagem_servidor(i, &estado);
                }
            }
        }
    }

    printf("\n[SENSOR] Encerrando sensor...\n");
    return 0;
}