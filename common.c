#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "common.h"

TipoServidor tipo_servidor;

/**
 * @brief Gera um ID aleatório para o sensor
 *
 * @param buffer Buffer onde armazenar o ID gerado
 * @param tamanho Tamanho do buffer (deve ser pelo menos TAMANHO_ID_SENSOR+1)
 *
 * @note Gera um número de TAMANHO_ID_SENSOR dígitos que não começa com zero
 */
void gerar_id_sensor(char *buffer, size_t tamanho)
{
    // Garante que o buffer tem tamanho suficiente
    if (tamanho < TAMANHO_ID_SENSOR + 1)
    {
        fprintf(stderr, "Buffer muito pequeno para ID\n");
        return;
    }

    // Inicializa o gerador de números aleatórios
    static int inicializado = 0;
    if (!inicializado)
    {
        srand(time(NULL));
        inicializado = 1;
    }

    // Gera cada dígito (o primeiro não pode ser zero)
    buffer[0] = '1' + (rand() % 9); // Primeiro dígito: 1-9
    for (int i = 1; i < TAMANHO_ID_SENSOR; i++)
    {
        buffer[i] = '0' + (rand() % 10); // Dígitos subsequentes: 0-9
    }
    buffer[TAMANHO_ID_SENSOR] = '\0'; // Terminador nulo
}

int criar_e_configurar_socket_escuta(int porta, int backlog)
{
    int socket_fd;
    struct sockaddr_in endereco_servidor;

    // Criação do socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR)
    {
        perror("Erro ao criar socket");
        return SOCKET_ERROR;
    }

    // Configuração do endereço
    memset(&endereco_servidor, 0, sizeof(endereco_servidor));
    endereco_servidor.sin_family = AF_INET;
    endereco_servidor.sin_addr.s_addr = INADDR_ANY;
    endereco_servidor.sin_port = htons(porta);

    // Opção para reutilizar endereço
    int opcao = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opcao, sizeof(opcao)) == SOCKET_ERROR)
    {
        perror("Erro ao configurar socket");
        close(socket_fd);
        return SOCKET_ERROR;
    }

    // Vinculação do socket ao endereço
    if (bind(socket_fd, (struct sockaddr *)&endereco_servidor, sizeof(endereco_servidor)) == SOCKET_ERROR)
    {
        perror("Erro ao vincular socket");
        close(socket_fd);
        return SOCKET_ERROR;
    }

    // Habilita escuta no socket
    if (listen(socket_fd, backlog) == SOCKET_ERROR)
    {
        perror("Erro ao escutar no socket");
        close(socket_fd);
        return SOCKET_ERROR;
    }

    return socket_fd;
}

void inicializar_link_p2p(const char *ip_peer_alvo, int porta_p2p,
                          int *socket_p2p_ptr, int *socket_escuta_p2p_ptr,
                          fd_set *conjunto_principal_ptr, int *fd_maximo_ptr)
{

    // Limpeza de conexões existentes
    if (*socket_p2p_ptr != -1)
    {
        FD_CLR(*socket_p2p_ptr, conjunto_principal_ptr);
        close(*socket_p2p_ptr);
        *socket_p2p_ptr = -1;
    }

    if (*socket_escuta_p2p_ptr != -1)
    {
        FD_CLR(*socket_escuta_p2p_ptr, conjunto_principal_ptr);
        close(*socket_escuta_p2p_ptr);
        *socket_escuta_p2p_ptr = -1;
    }

    // Tentativa de conexão ativa com o peer
    int socket_temp_p2p;
    if ((socket_temp_p2p = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR)
    {
        return;
    }

    struct sockaddr_in endereco_peer;
    memset(&endereco_peer, 0, sizeof(endereco_peer));
    endereco_peer.sin_family = AF_INET;
    endereco_peer.sin_port = htons(porta_p2p);

    if (inet_pton(AF_INET, ip_peer_alvo, &endereco_peer.sin_addr) <= 0)
    {
        close(socket_temp_p2p);
        return;
    }

    // Tentativa de conexão
    if (connect(socket_temp_p2p, (struct sockaddr *)&endereco_peer, sizeof(endereco_peer)) == 0)
    {
        *socket_p2p_ptr = socket_temp_p2p;
        FD_SET(*socket_p2p_ptr, conjunto_principal_ptr);

        if (*socket_p2p_ptr > *fd_maximo_ptr)
        {
            *fd_maximo_ptr = *socket_p2p_ptr;
        }

        char buffer_envio[MAX_MSG_SIZE];
        construir_mensagem(buffer_envio, MAX_MSG_SIZE, REQ_CONNPEER, NULL, NULL);
        printf("[P2P] Enviando REQ_CONNPEER\n");
        send(*socket_p2p_ptr, buffer_envio, strlen(buffer_envio), 0);
        return;
    }

    // Se a conexão ativa falhou, fecha o socket temporário
    close(socket_temp_p2p);

    printf("No peer found, starting to listen...\n");

    // Configuração do socket de escuta para conexões entrantes
    int backlog_p2p = 1; // Aceita apenas 1 conexão pendente
    *socket_escuta_p2p_ptr = criar_e_configurar_socket_escuta(porta_p2p, backlog_p2p);

    if (*socket_escuta_p2p_ptr == SOCKET_ERROR)
    {
        fprintf(stderr, "Erro crítico ao criar socket de escuta P2P.\n");
        return;
    }

    FD_SET(*socket_escuta_p2p_ptr, conjunto_principal_ptr);
    if (*socket_escuta_p2p_ptr > *fd_maximo_ptr)
    {
        *fd_maximo_ptr = *socket_escuta_p2p_ptr;
    }
}

void construir_mensagem(char *buffer_destino, size_t tamanho_buffer, int codigo,
                        const char *payload1, const char *payload2)
{
    // 1. Limpeza do buffer para garantir que não haja dados residuais
    memset(buffer_destino, 0, tamanho_buffer);

    size_t bytes_escritos = 0;

    // 2. Construção da mensagem conforme os parâmetros fornecidos
    if (payload1 != NULL && payload2 != NULL)
    {
        bytes_escritos = snprintf(buffer_destino, tamanho_buffer, "%d %s %s", codigo, payload1, payload2);
    }
    else if (payload1 != NULL)
    {
        bytes_escritos = snprintf(buffer_destino, tamanho_buffer, "%d %s", codigo, payload1);
    }
    else if (payload2 != NULL)
    {
        bytes_escritos = snprintf(buffer_destino, tamanho_buffer, "%d %s", codigo, payload2);
    }
    else
    {
        bytes_escritos = snprintf(buffer_destino, tamanho_buffer, "%d", codigo);
    }

    // 3. Verificação se a mensagem foi truncada
    if (bytes_escritos >= tamanho_buffer)
    {
        fprintf(stderr, "[SERVER] AVISO: Mensagem truncada (limite de %zu bytes).\n", tamanho_buffer);
    }
}

void processar_mensagem_recebida(int socket_origem, const char *buffer_recebido,
                                 int *handshake_completo_ptr, fd_set *conjunto_principal_ptr,
                                 int *socket_p2p_ptr, SensorInfo *sensores,
                                 int *contador_sensores_ptr,
                                 PendingRequest *pedidos_pendentes)
{
    int codigo_recebido;
    char payload1[256], payload2[256];
    char buffer_envio[MAX_MSG_SIZE];

    ParseResultType resultado = analisar_mensagem(buffer_recebido, &codigo_recebido, payload1, payload2);

    if (resultado == PARSE_ERROR_INVALID_FORMAT)
    {
        fprintf(stderr, "[SERVER] Mensagem com formato inválido: \"%s\"\n", buffer_recebido);
        return;
    }

    switch (codigo_recebido)
    {
    case REQ_CONNSEN: // Código 23
        if (*contador_sensores_ptr >= MAX_CLIENTS)
            tratar_erro_limite_clientes(socket_origem, buffer_envio, conjunto_principal_ptr);
        else
            registrar_novo_sensor(sensores, socket_origem, payload1,
                                  contador_sensores_ptr, buffer_envio);
        break;

    case REQ_DISCSEN: // 25
        printf("[SERVER] Recebido REQ_DISCSEN do sensor ID %s (socket: %d).\n", payload1, socket_origem);
        tratar_desconexao_sensor(sensores, payload1, buffer_envio, socket_origem,
                                 contador_sensores_ptr, conjunto_principal_ptr);
        break;

    case REQ_SENSLOC: // 38
        if (tipo_servidor == TIPO_SERVIDOR_LOCALIZACAO)
            tratar_metodo_localizar(sensores, payload1, buffer_envio, socket_origem);
        break;

    case 40: // REQ_SENSSTATUS e REQ_LOCLIST
        if (tipo_servidor == TIPO_SERVIDOR_STATUS)
        {
            tratar_comando_verificar_falha(socket_origem, payload1, sensores,
                                           socket_p2p_ptr, pedidos_pendentes);
        }
        else if (tipo_servidor == TIPO_SERVIDOR_LOCALIZACAO)
        {
            tratar_comando_diagnosticar(payload2, sensores, socket_origem, buffer_envio);
        }
        break;

    case REQ_CONNPEER: // 20
        printf("[P2P] REQ_CONNPEER recebido\n");
        construir_mensagem(buffer_envio, MAX_MSG_SIZE, RES_CONNPEER, "PEER_ID_1", NULL);
        send(socket_origem, buffer_envio, strlen(buffer_envio), 0);
        break;

    case RES_CONNPEER: // 21
        if (*handshake_completo_ptr == 1)
            return;

        if (tipo_servidor == TIPO_SERVIDOR_STATUS)
        {
            printf("[P2P] Novo Peer ID: %s.\n", payload1);
            construir_mensagem(buffer_envio, MAX_MSG_SIZE, RES_CONNPEER, "PEER_ID_SS", NULL);
            send(socket_origem, buffer_envio, strlen(buffer_envio), 0);
        }
        else
        {
            printf("[P2P] Novo Peer ID: %s.\n", payload1);
        }
        *handshake_completo_ptr = 1;
        break;

    case REQ_DISCPEER: // 22
        printf("[P2P] REQ_DISCPEER recebido\n");
        if (strcmp(payload1, "PEER_ID_1") == 0)
        {
            construir_mensagem(buffer_envio, MAX_MSG_SIZE, MSG_ERROR, "2", "Peer não encontrado");
            send(socket_origem, buffer_envio, strlen(buffer_envio), 0);
            return;
        }

        construir_mensagem(buffer_envio, MAX_MSG_SIZE, MSG_OK, "1", "Peer PEER_ID_1 desconectado");
        send(socket_origem, buffer_envio, strlen(buffer_envio), 0);

        printf("[P2P] Encerrando conexão P2P (socket: %d)\n", socket_origem);
        close(socket_origem);
        FD_CLR(socket_origem, conjunto_principal_ptr);
        *socket_p2p_ptr = -1;
        *handshake_completo_ptr = 0;
        break;

    case REQ_CHECKALERT: // 36 (SS -> SL)
        if (tipo_servidor == TIPO_SERVIDOR_LOCALIZACAO)
            tratar_pedido_verificar_alerta(sensores, payload1, socket_origem, buffer_envio);
        break;

    case RES_CHECKALERT: // 37 (SL -> SS)
        if (tipo_servidor == TIPO_SERVIDOR_STATUS)
            tratar_resposta_verificar_alerta(payload1,
                                             buffer_envio, pedidos_pendentes);
        break;

    case MSG_OK:
        printf("[P2P] %s \n", payload2);
        break;

    case MSG_ERROR: // 255
        if (socket_origem == *socket_p2p_ptr)
        {
            printf("[SS] Erro recebido do SL (peer).\n");

            int slot_pendente = -1;
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (pedidos_pendentes[i].is_active)
                {
                    slot_pendente = i;
                    break;
                }
            }
            if (slot_pendente != -1)
            {
                int socket_cliente = pedidos_pendentes[slot_pendente].original_client_fd;
                printf("[SS] Encaminhando erro para cliente (socket: %d).\n", socket_cliente);
                construir_mensagem(buffer_envio, sizeof(buffer_envio), MSG_ERROR, payload1, NULL);
                send(socket_cliente, buffer_envio, strlen(buffer_envio), 0);

                pedidos_pendentes[slot_pendente].is_active = 0;
            }
        }
        break;

    default:
        printf("[SERVER] Código desconhecido (%d) recebido do socket %d\n",
               codigo_recebido, socket_origem);
        break;
    }
}

ParseResultType analisar_mensagem(const char *buffer_entrada, int *codigo_destino,
                                  char *payload1_destino, char *payload2_destino)
{
    // Limpeza inicial dos buffers de destino
    if (payload1_destino)
        payload1_destino[0] = '\0';
    if (payload2_destino)
        payload2_destino[0] = '\0';
    if (codigo_destino)
        *codigo_destino = -1;

    int itens_lidos = 0;

    // Tenta analisar a mensagem nos formatos esperados
    // Formato: Código Payload1 Payload2
    itens_lidos = sscanf(buffer_entrada, "%d %s %s", codigo_destino,
                         payload1_destino, payload2_destino);
    if (itens_lidos == 3)
    {
        return PARSE_SUCCESS_TWO_PAYLOADS;
    }

    // Formato: Código Payload1
    itens_lidos = sscanf(buffer_entrada, "%d %s", codigo_destino, payload1_destino);
    if (itens_lidos == 2)
    {
        return PARSE_SUCCESS_ONE_PAYLOAD;
    }

    // Formato: Código
    itens_lidos = sscanf(buffer_entrada, "%d", codigo_destino);
    if (itens_lidos == 1)
    {
        return PARSE_SUCCESS_CODE_ONLY;
    }

    // Formato não reconhecido
    fprintf(stderr, "[PARSER] Erro: Formato inválido na mensagem \"%s\"\n", buffer_entrada);
    return PARSE_ERROR_INVALID_FORMAT;
}

void tratar_erro_limite_clientes(int socket_cliente, char buffer_envio[MAX_MSG_SIZE],
                                 fd_set *conjunto_principal_ptr)
{
    printf("[SERVER] Limite máximo de sensores atingido\n");

    // Constroi e envia mensagem de erro
    construir_mensagem(buffer_envio, MAX_MSG_SIZE, MSG_ERROR, "9", "Limite de sensores excedido");
    send(socket_cliente, buffer_envio, strlen(buffer_envio), 0);

    // Encerra a conexão
    close(socket_cliente);
    FD_CLR(socket_cliente, conjunto_principal_ptr);
}

void registrar_novo_sensor(SensorInfo *sensores, int socket_cliente, char localizacao[256],
                           int *contador_sensores_ptr,
                           char buffer_envio[MAX_MSG_SIZE])
{
    // Encontra um slot vazio no array de sensores
    int indice_slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (sensores[i].is_active == 0)
        {
            indice_slot = i;
            break;
        }
    }

    if (indice_slot != -1)
    {
        // Configura os dados do novo sensor
        sensores[indice_slot].is_active = 1;
        sensores[indice_slot].socket_fd = socket_cliente;
        sensores[indice_slot].location_id = atoi(localizacao);

        // Gera status de risco aleatório (0 ou 1)
        srand(time(NULL));
        sensores[indice_slot].risk_status = rand() % 2;

        gerar_id_sensor(sensores[indice_slot].sensor_id_str,
                        sizeof(sensores[indice_slot].sensor_id_str));

        (contador_sensores_ptr)++;

        // Log do registro
        const char *tipo_servidor_sigla = (tipo_servidor == TIPO_SERVIDOR_STATUS) ? "SS" : "SL";
        printf("[%s] Novo sensor registrado: %s\n", tipo_servidor_sigla,
               sensores[indice_slot].sensor_id_str);

        // Envia confirmação para o sensor
        construir_mensagem(buffer_envio, MAX_MSG_SIZE, RES_CONNSEN,
                           sensores[indice_slot].sensor_id_str, NULL);
        send(socket_cliente, buffer_envio, strlen(buffer_envio), 0);
    }
    else
    {
        // Erro crítico - não deveria acontecer
        fprintf(stderr, "[SERVER] ERRO CRÍTICO: Contador de sensores dessincronizado\n");
    }
}

void tratar_desconexao_sensor(SensorInfo *sensores, char id_sensor[256],
                              char buffer_envio[MAX_MSG_SIZE], int socket_cliente,
                              int *contador_sensores_ptr, fd_set *conjunto_principal_ptr)
{
    const char *tipo_servidor_sigla = (tipo_servidor == TIPO_SERVIDOR_STATUS) ? "SS" : "SL";
    SensorInfo *sensor = buscar_sensor_por_id(sensores, id_sensor);

    if (sensor == NULL)
    {
        fprintf(stderr, "[%s] Sensor não encontrado para desconexão (ID: %s).\n",
                tipo_servidor_sigla, id_sensor);
        construir_mensagem(buffer_envio, MAX_MSG_SIZE, MSG_ERROR, "10", "Sensor não encontrado");
        send(socket_cliente, buffer_envio, strlen(buffer_envio), 0);
        return;
    }

    // Envia confirmação de desconexão
    construir_mensagem(buffer_envio, MAX_MSG_SIZE, MSG_OK, "1", tipo_servidor_sigla);
    send(socket_cliente, buffer_envio, strlen(buffer_envio), 0);

    // Atualiza estado do sensor
    sensor->is_active = 0;
    (*contador_sensores_ptr)--;

    // Libera recursos
    close(socket_cliente);
    FD_CLR(socket_cliente, conjunto_principal_ptr);

    printf("[%s] Sensor desconectado: %s\n", tipo_servidor_sigla, id_sensor);
}

void tratar_metodo_localizar(SensorInfo *sensores, char id_sensor[256],
                             char buffer_envio[MAX_MSG_SIZE], int socket_cliente)
{
    printf("[SL] REQ_SENSLOC %s\n", id_sensor);

    SensorInfo *sensor = buscar_sensor_por_id(sensores, id_sensor);

    if (sensor != NULL)
    {
        char localizacao_str[5];
        snprintf(localizacao_str, sizeof(localizacao_str), "%d", sensor->location_id);
        construir_mensagem(buffer_envio, MAX_MSG_SIZE, RES_SENSLOC,
                           sensor->sensor_id_str, localizacao_str);
    }
    else
    {
        construir_mensagem(buffer_envio, MAX_MSG_SIZE, MSG_ERROR,
                           "10", "Sensor_not_found");
    }

    send(socket_cliente, buffer_envio, strlen(buffer_envio), 0);
}

void tratar_comando_verificar_falha(int socket_cliente, char id_sensor[256],
                                    SensorInfo *sensores, int *socket_p2p_ptr,
                                    PendingRequest *pedidos_pendentes)
{
    printf("[SS] REQ_SENSSTATUS %s\n", id_sensor);

    char buffer_envio[MAX_MSG_SIZE];
    SensorInfo *sensor = buscar_sensor_por_id(sensores, id_sensor);

    // Sensor não encontrado
    if (sensor == NULL)
    {
        construir_mensagem(buffer_envio, MAX_MSG_SIZE, MSG_ERROR, "10", "Sensor não encontrado");
        send(socket_cliente, buffer_envio, strlen(buffer_envio), 0);
        return;
    }

    // Sensor inativo
    if (sensor->is_active == 0)
    {
        construir_mensagem(buffer_envio, MAX_MSG_SIZE, MSG_OK, "03", "Status do sensor 0");
        send(socket_cliente, buffer_envio, strlen(buffer_envio), 0);
        return;
    }

    printf("[SS] Sensor %s status = 1 (failure detected) \n", id_sensor);

    // Verifica conexão P2P ativa
    if (socket_p2p_ptr != NULL && *socket_p2p_ptr != -1)
    {
        printf("[SS] Encaminhando REQ_CHECKALERT %s para SL\n", id_sensor);

        // Encontra slot para pedido pendente
        int slot_pendente = -1;
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (pedidos_pendentes[i].is_active == 0)
            {
                slot_pendente = i;
                break;
            }
        }

        if (slot_pendente != -1)
        {
            // Registra pedido pendente
            pedidos_pendentes[slot_pendente].is_active = 1;
            pedidos_pendentes[slot_pendente].original_client_fd = socket_cliente;
            strncpy(pedidos_pendentes[slot_pendente].sensor_id_in_query, id_sensor,
                    sizeof(pedidos_pendentes[slot_pendente].sensor_id_in_query) - 1);

            // Envia requisição para SL
            construir_mensagem(buffer_envio, MAX_MSG_SIZE, REQ_CHECKALERT, id_sensor, NULL);
            send(*socket_p2p_ptr, buffer_envio, strlen(buffer_envio), 0);
        }
        else
        {
            construir_mensagem(buffer_envio, MAX_MSG_SIZE, MSG_ERROR, "0", "Sem slots disponíveis");
            send(socket_cliente, buffer_envio, strlen(buffer_envio), 0);
        }
        return;
    }

    // Sem conexão P2P disponível
    construir_mensagem(buffer_envio, MAX_MSG_SIZE, MSG_ERROR, "2", "Peer não encontrado");
    send(socket_cliente, buffer_envio, strlen(buffer_envio), 0);
}

void tratar_comando_diagnosticar(char localizacao[256], SensorInfo *sensores,
                                 int socket_cliente, char buffer_envio[MAX_MSG_SIZE])
{
    printf("[SL] Recebido pedido de diagnóstico para localização %s\n", localizacao);

    char lista_sensores[MAX_MSG_SIZE - 10] = "";
    int localizacao_alvo = atoi(localizacao);
    int sensores_encontrados = 0;

    // Procura sensores na localização especificada
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (sensores[i].is_active && sensores[i].location_id == localizacao_alvo)
        {
            if (sensores_encontrados > 0)
            {
                strncat(lista_sensores, ",", sizeof(lista_sensores) - strlen(lista_sensores) - 1);
            }

            strncat(lista_sensores, sensores[i].sensor_id_str,
                    sizeof(lista_sensores) - strlen(lista_sensores) - 1);
            sensores_encontrados++;
        }
    }

    // Monta resposta conforme encontrou sensores ou não
    if (sensores_encontrados > 0)
    {
        printf("[SL] Encontrados %d sensores na localização %d\n", sensores_encontrados, localizacao_alvo);

        char localizacao_str[16];
        snprintf(localizacao_str, sizeof(localizacao_str), "%d", localizacao_alvo);

        construir_mensagem(buffer_envio, MAX_MSG_SIZE, RES_LOCLIST,
                           localizacao_str, lista_sensores);
    }
    else
    {
        printf("[SL] Nenhum sensor encontrado na localização %d\n", localizacao_alvo);
        construir_mensagem(buffer_envio, MAX_MSG_SIZE, MSG_ERROR,
                           "10", "Sensor não encontrado");
    }

    send(socket_cliente, buffer_envio, strlen(buffer_envio), 0);
}

void tratar_pedido_verificar_alerta(SensorInfo *sensores, char id_sensor[256],
                                    int socket_cliente, char buffer_envio[MAX_MSG_SIZE])
{
    printf("[SL] REQ_CHECKALERT %s\n", id_sensor);

    SensorInfo *sensor = buscar_sensor_por_id(sensores, id_sensor);

    if (sensor != NULL)
    {
        char localizacao_str[5];
        snprintf(localizacao_str, sizeof(localizacao_str), "%d", sensor->location_id);

        printf("[SL] Found location of sensor %s: location %s\n", id_sensor, localizacao_str);
        printf("[SL] Sending RES_CHECKALERT %s para SS\n", localizacao_str);

        construir_mensagem(buffer_envio, MAX_MSG_SIZE, RES_CHECKALERT, localizacao_str, NULL);
    }
    else
    {
        printf("[SL] Sensor %s não encontrado\n", id_sensor);
        construir_mensagem(buffer_envio, MAX_MSG_SIZE, MSG_ERROR, "10", "Sensor não encontrado");
    }

    send(socket_cliente, buffer_envio, strlen(buffer_envio), 0);
}

void tratar_resposta_verificar_alerta(const char *localizacao,
                                      char *buffer_envio,
                                      PendingRequest *pedidos_pendentes)
{
    printf("[SS] RES_CHECKALERT %s\n", localizacao);

    // Encontra pedido pendente correspondente
    int slot_pendente = -1;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (pedidos_pendentes[i].is_active)
        {
            slot_pendente = i;
            break;
        }
    }

    if (slot_pendente != -1)
    {
        int socket_cliente = pedidos_pendentes[slot_pendente].original_client_fd;

        // Encaminha resposta para o cliente original
        construir_mensagem(buffer_envio, MAX_MSG_SIZE, RES_SENSSTATUS, localizacao, NULL);
        send(socket_cliente, buffer_envio, strlen(buffer_envio), 0);

        // Marca pedido como concluído
        pedidos_pendentes[slot_pendente].is_active = 0;
        printf("[SS] Sending RES_SENSSTATUS %d to CLIENT\n", socket_cliente);
    }
    else
    {
        fprintf(stderr, "[SS] ERRO: Nenhum pedido pendente encontrado para a resposta\n");
    }
}

SensorInfo *buscar_sensor_por_id(SensorInfo *sensores, const char *id_sensor)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (sensores[i].is_active && strcmp(sensores[i].sensor_id_str, id_sensor) == 0)
        {
            return &sensores[i];
        }
    }

    return NULL;
}

int conectar_servidor(const char *ip_servidor, int porta)
{
    int socket_servidor;
    struct sockaddr_in endereco_servidor;

    // Criação do socket
    if ((socket_servidor = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR)
    {
        perror("[REDE] Erro ao criar socket");
        return SOCKET_ERROR;
    }

    // Configuração do endereço do servidor
    memset(&endereco_servidor, 0, sizeof(endereco_servidor));
    endereco_servidor.sin_family = AF_INET;
    endereco_servidor.sin_port = htons(porta);

    // Conversão do endereço IP
    if (inet_pton(AF_INET, ip_servidor, &endereco_servidor.sin_addr) <= 0)
    {
        perror("[REDE] Erro na conversão de endereço");
        close(socket_servidor);
        return SOCKET_ERROR;
    }

    // Tentativa de conexão
    if (connect(socket_servidor, (struct sockaddr *)&endereco_servidor, sizeof(endereco_servidor)) == SOCKET_ERROR)
    {
        perror("[REDE] Erro na conexão");
        close(socket_servidor);
        return SOCKET_ERROR;
    }

    return socket_servidor;
}

ParseResultType analisar_mensagem_sensor(const char *buffer, int *codigo,
                                         char *payload1, char *payload2)
{
    char copia[MAX_MSG_SIZE];

    // Cria cópia segura para manipulação
    strncpy(copia, buffer, sizeof(copia) - 1);
    copia[sizeof(copia) - 1] = '\0';

    // Inicializa buffers de saída
    if (payload1)
        payload1[0] = '\0';
    if (payload2)
        payload2[0] = '\0';
    if (codigo)
        *codigo = -1;

    // Extrai o código da mensagem
    char *token, *final_ptr;
    token = strtok(copia, " \t\n\r"); // Ignora espaços em branco

    if (!token)
        return PARSE_ERROR_INVALID_FORMAT;

    *codigo = strtol(token, &final_ptr, 10);
    if (*final_ptr != '\0')
        return PARSE_ERROR_INVALID_FORMAT;

    // Tenta extrair primeiro payload
    token = strtok(NULL, " \t\n\r");
    if (!token)
        return PARSE_SUCCESS_CODE_ONLY;

    if (payload1)
        strncpy(payload1, token, 255);
    payload1[255] = '\0'; // Garante terminação nula

    // Tenta extrair segundo payload
    token = strtok(NULL, " \t\n\r");
    if (!token)
        return PARSE_SUCCESS_ONE_PAYLOAD;

    if (payload2)
        strncpy(payload2, token, 255);
    payload2[255] = '\0'; // Garante terminação nula

    // Verifica se há conteúdo extra não esperado
    if (strtok(NULL, " \t\n\r"))
        return PARSE_ERROR_INVALID_FORMAT;

    return PARSE_SUCCESS_TWO_PAYLOADS;
}

void processar_mensagem_servidor(int socket_servidor, EstadoSensor *estado)
{
    ssize_t bytes_recebidos = recv(socket_servidor, estado->buffer_recebido, sizeof(estado->buffer_recebido) - 1, 0);

    // Verifica se a conexão foi encerrada
    if (bytes_recebidos <= 0)
    {
        if (socket_servidor == estado->socket_sl)
            estado->socket_sl = -1;
        if (socket_servidor == estado->socket_ss)
            estado->socket_ss = -1;
        return;
    }

    estado->buffer_recebido[bytes_recebidos] = '\0';

    int codigo;
    char payload1[256], payload2[256];
    analisar_mensagem_sensor(estado->buffer_recebido, &codigo, payload1, payload2);

    switch (codigo)
    {
    case RES_CONNSEN:
        if (socket_servidor == estado->socket_sl)
        {
            strncpy(estado->id_sensor_sl, payload1, sizeof(estado->id_sensor_sl) - 1);
            estado->registrado_sl = 1;
            printf("[CLIENT] Novo ID registrado no SL: %s\n", estado->id_sensor_sl);
        }
        else if (socket_servidor == estado->socket_ss)
        {
            strncpy(estado->id_sensor_ss, payload1, sizeof(estado->id_sensor_ss) - 1);
            estado->registrado_ss = 1;
            printf("[CLIENT] Novo ID registrado no SS: %s\n", estado->id_sensor_ss);
        }
        break;

    case 41: // RES_LOCLIST e RES_SENSSTATUS
        if (payload1[0] != '\0' && payload2[0] != '\0')
        {
            printf("[CLIENT] Sensores na localização %s: %s\n", payload1, payload2);
        }
        else
        {
            int id_localizacao = atoi(payload1);
            printf("[CLIENT] Alert received from area: %d (%s)\n",
                   id_localizacao, obter_nome_regiao(id_localizacao));
        }
        break;

    case RES_SENSLOC:
        printf("[CLIENT] Localização atual do sensor %s: %s\n", payload1, payload2);
        break;

    case MSG_ERROR:
        if (strcmp(payload1, "10") == 0)
        {
            printf("[CLIENT] Sensor not found\n");
        }
        else
        {
            printf("[CLIENT] %s\n", payload2);
        }
        break;

    case MSG_OK:
        if (payload1[0] == '1') // Confirmação de desconexão
        {
            printf("[CLIENT] Desconexão bem-sucedida%s\n", payload2);
            if (strcmp(payload2, "SS") == 0)
            {
                estado->socket_ss = -1;
                estado->registrado_ss = 0;
            }
            else if (strcmp(payload2, "SL") == 0)
            {
                estado->socket_sl = -1;
                estado->registrado_sl = 0;
            }

            if (estado->socket_sl == -1 && estado->socket_ss == -1)
            {
                printf("[SENSOR] Ambas conexões encerradas. Saindo...\n");
                exit(0);
            }
        }
        printf("[SENSOR] %s\n", payload2);
        break;

    default:
        printf("[SENSOR] Código não reconhecido: %d\n", codigo);
        break;
    }
}

const char *obter_nome_regiao(int id_localizacao)
{
    if (id_localizacao >= 1 && id_localizacao <= 3)
        return "Norte";
    if (id_localizacao >= 4 && id_localizacao <= 5)
        return "Sul";
    if (id_localizacao >= 6 && id_localizacao <= 7)
        return "Leste";
    if (id_localizacao >= 8 && id_localizacao <= 10)
        return "Oeste";
    return "Área não coberta";
}