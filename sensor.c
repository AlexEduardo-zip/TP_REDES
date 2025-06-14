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

// ParseResultType parse_message(const char *buffer, int *code, char *p1, char *p2)
// {
//     char copy[MAX_MSG_SIZE];
//     strncpy(copy, buffer, sizeof(copy) - 1);
//     copy[sizeof(copy) - 1] = '\0';
//     if (p1)
//         p1[0] = '\0';
//     if (p2)
//         p2[0] = '\0';
//     if (code)
//         *code = -1;
//     char *token, *endptr;
//     token = strtok(copy, " \t\n\r");
//     if (!token)
//         return PARSE_ERROR_INVALID_FORMAT;
//     *code = strtol(token, &endptr, 10);
//     if (*endptr != '\0')
//         return PARSE_ERROR_INVALID_FORMAT;
//     token = strtok(NULL, " \t\n\r");
//     if (!token)
//         return PARSE_SUCCESS_CODE_ONLY;
//     if (p1)
//         strncpy(p1, token, 255);
//     token = strtok(NULL, " \t\n\r");
//     if (!token)
//         return PARSE_SUCCESS_ONE_PAYLOAD;
//     if (p2)
//         strncpy(p2, token, 255);
//     if (strtok(NULL, " \t\n\r"))
//         return PARSE_ERROR_INVALID_FORMAT;
//     return PARSE_SUCCESS_TWO_PAYLOADS;
// }

// const char *get_region_name(int loc_id)
// {
//     if (loc_id >= 1 && loc_id <= 3)
//         return "Norte";
//     if (loc_id >= 4 && loc_id <= 5)
//         return "Sul";
//     if (loc_id >= 6 && loc_id <= 7)
//         return "Leste";
//     if (loc_id >= 8 && loc_id <= 10)
//         return "Oeste";
//     return "Outside covered zone";
//     ;
// }

// void process_keyboard_input(SensorState *state)
// {
//     char command_buffer[MAX_MSG_SIZE];
//     if (fgets(command_buffer, sizeof(command_buffer), stdin) == NULL)
//     {
//         printf("\n[CLIENT] EOF detectado. A preparar para encerrar...\n");
//         build_message(state->send_buffer, sizeof(state->send_buffer), REQ_DISCSEN, state->sl_sensor_id, NULL);
//         send(state->sl_fd, state->send_buffer, strlen(state->send_buffer), 0);
//         build_message(state->send_buffer, sizeof(state->send_buffer), REQ_DISCSEN, state->ss_sensor_id, NULL);
//         send(state->ss_fd, state->send_buffer, strlen(state->send_buffer), 0);
//         return;
//     }
//     command_buffer[strcspn(command_buffer, "\n")] = 0;

//     char temp_copy[MAX_MSG_SIZE];
//     strncpy(temp_copy, command_buffer, sizeof(temp_copy));
//     char *command = strtok(temp_copy, " ");
//     if (!command)
//         return;

//     if (strcmp(command, "kill") == 0)
//     {

//         printf("[CLIENT] kill\n");
//         printf("[CLIENT] Sending REQ_DISCSEN %s\n", state->ss_sensor_id);
//         build_message(state->send_buffer, sizeof(state->send_buffer), REQ_DISCSEN, state->sl_sensor_id, NULL);
//         send(state->sl_fd, state->send_buffer, strlen(state->send_buffer), 0);
//         build_message(state->send_buffer, sizeof(state->send_buffer), REQ_DISCSEN, state->ss_sensor_id, NULL);
//         send(state->ss_fd, state->send_buffer, strlen(state->send_buffer), 0);
//     }
//     else if (strcmp(command, "check") == 0)
//     {
//         char *arg = strtok(NULL, "");
//         if (arg && strcmp(arg, "failure") == 0)
//         {
//             printf("[CLIENT] check failure\n");
//             printf("[CLIENT] Sending REQ_SENSSTATUS %s\n", state->ss_sensor_id);
//             build_message(state->send_buffer, sizeof(state->send_buffer), REQ_SENSSTATUS, state->ss_sensor_id, NULL);
//             send(state->ss_fd, state->send_buffer, strlen(state->send_buffer), 0);
//         }
//     }
//     else if (strcmp(command, "locate") == 0)
//     {
//         char *target_id = strtok(NULL, " ");
//         if (target_id)
//         {
//             printf("[CLIENT] located %s\n", target_id);
//             printf("[CLIENT] Sending REQ_SENSLOC %s\n", target_id);

//             build_message(state->send_buffer, sizeof(state->send_buffer), REQ_SENSLOC, target_id, NULL);
//             send(state->sl_fd, state->send_buffer, strlen(state->send_buffer), 0);
//         }
//     }
//     else if (strcmp(command, "diagnose") == 0)
//     {
//         char *target_loc = strtok(NULL, " ");
//         if (target_loc)
//         {
//             printf("[CLIENT] diagnose %s\n", target_loc);
//             printf("[CLIENT] Sending REQ_LOCLIST %s\n", target_loc);
//             build_message(state->send_buffer, sizeof(state->send_buffer), REQ_LOCLIST, state->sl_sensor_id, target_loc);
//             send(state->sl_fd, state->send_buffer, strlen(state->send_buffer), 0);
//         }
//     }
//     else
//     {
//         printf("[CLIENT] Command not found: '%s'\n", command);
//     }
// }

// void process_server_message(int server_fd, SensorState *state)
// {
//     ssize_t bytes = recv(server_fd, state->recv_buffer, sizeof(state->recv_buffer) - 1, 0);
//     if (bytes <= 0)
//     {
//         if (server_fd == state->sl_fd)
//             state->sl_fd = -1;
//         if (server_fd == state->ss_fd)
//             state->ss_fd = -1;
//         return;
//     }
//     state->recv_buffer[bytes] = '\0';

//     int code;
//     char p1[256], p2[256];
//     parse_message(state->recv_buffer, &code, p1, p2);

//     switch (code)
//     {
//     case RES_CONNSEN:
//         if (server_fd == state->sl_fd)
//         {
//             strncpy(state->sl_sensor_id, p1, sizeof(state->sl_sensor_id) - 1);
//             state->registered_on_sl = 1;
//             printf("[CLIENTE] SS New ID:: %s\n", state->sl_sensor_id);
//         }
//         else if (server_fd == state->ss_fd)
//         {
//             strncpy(state->ss_sensor_id, p1, sizeof(state->ss_sensor_id) - 1);
//             state->registered_on_ss = 1;
//             printf("[CLIENTE] SL New ID:: %s\n", state->ss_sensor_id);
//         }
//         break;
//     case 41: // RES_LOCLIST && RES_SENSSTATUS
//         if (p1[0] != '\0' && p2[0] != '\0')
//         {
//             printf("[CLIENT] Sensors at location %s:  %s\n", p1, p2);
//         }
//         else
//         {
//             int loc_id = atoi(p1);
//             printf("[CLIENT] Alert received from area: %d (%s)\n", loc_id, get_region_name(loc_id));
//         }
//         break;
//     case RES_SENSLOC:
//         printf("[CLIENT] Current sensor %s location: %s\n", p1, p2);
//         break;
//     case MSG_ERROR:
//         printf("[CLIENT] %s\n", p2);
//         break;
//     case MSG_OK:
//         if (p1[0] == '1')
//         {
//             printf("[CLIENT] Successful disconnect%s\n", p2);
//             if (strcmp(p2, "SS") == 0)
//             {
//                 state->ss_fd = -1;
//                 state->registered_on_ss = 0;
//             }
//             else if (strcmp(p2, "SL") == 0)
//             {
//                 state->sl_fd = -1;
//                 state->registered_on_sl = 0;
//             }

//             if (state->sl_fd == -1 && state->ss_fd == -1)
//             {
//                 printf("[CLIENT] Both connections closed. Exiting...\n");
//                 exit(0);
//             }
//         }
//         printf("[CLIENT] %s\n", p2);
//         break;
//     default:
//         printf("[CLIENT] Code not founded: %d\n", code);
//         break;
//     }
// }

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
                        processar_entrada_teclado(&estado);
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