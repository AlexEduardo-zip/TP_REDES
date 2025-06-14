#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include "common.h"
#include <stddef.h>
#include <time.h>

int main(int argc, char *argv[])
{
    // Validação dos argumentos de entrada
    if (argc < 4)
    {
        fprintf(stderr, "Uso: %s <ip_peer> <porta_p2p> <porta_escuta_clientes>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *ip_peer_alvo = argv[1];
    int porta_p2p_comum = atoi(argv[2]);
    int porta_escuta_clientes = atoi(argv[3]);
    // Fim da validação dos argumentos

    // Definição do tipo de servidor
    if (porta_escuta_clientes == 60000)
    {
        tipo_servidor = TIPO_SERVIDOR_LOCALIZACAO;
        printf("[SL] iniciado na porta %d\n", porta_escuta_clientes);
    }
    else if (porta_escuta_clientes == 61000)
    {
        tipo_servidor = TIPO_SERVIDOR_STATUS;
        printf("[SS] iniciado na porta %d\n", porta_escuta_clientes);
    }
    else
    {
        perror("Porta inválida. Use 60000 para SL e 61000 para SS");
        exit(EXIT_FAILURE);
    }
    // Fim da definição do tipo de servidor

    // Configuração inicial dos sensores
    SensorInfo sensores_conectados[MAX_CLIENTS];
    PendingRequest pedidos_pendentes[MAX_CLIENTS];
    int contador_sensores = 0;

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        sensores_conectados[i].is_active = 0;
        sensores_conectados[i].socket_fd = -1;
        pedidos_pendentes[i].is_active = 0;
    }
    // Fim da configuração dos sensores

    // Configuração P2P
    // Descritor de socket para a conexão P2P ativa
    // Valor -1 indica que não há conexão ativa
    int socket_p2p = -1;

    // Descritor de socket para escuta de conexões P2P
    // Valor -1 indica que não está escutando conexões
    int socket_escuta_p2p = -1;

    // Flag que indica se o handshake P2P foi completado
    int handshake_p2p_completo = 0;

    // Socket principal para escuta de clientes
    int socket_escuta;

    // Número máximo de conexões pendentes na fila
    int max_conexoes_pendentes = 10;

    socket_escuta = criar_e_configurar_socket_escuta(porta_escuta_clientes, max_conexoes_pendentes);
    if (socket_escuta == SOCKET_ERROR)
    {
        perror("Falha crítica em criar_e_configurar_socket_escuta");
        exit(EXIT_FAILURE);
    }

    // Conjuntos de descritores de arquivo para select()
    fd_set conjunto_principal, conjunto_leitura;

    // Maior descritor de arquivo no conjunto
    int fd_maximo;

    FD_ZERO(&conjunto_principal);
    FD_ZERO(&conjunto_leitura);

    FD_SET(socket_escuta, &conjunto_principal);
    FD_SET(STDIN_FILENO, &conjunto_principal);

    fd_maximo = socket_escuta;

    inicializar_link_p2p(ip_peer_alvo, porta_p2p_comum,
                         &socket_p2p, &socket_escuta_p2p,
                         &conjunto_principal, &fd_maximo);

    // Loop principal do servidor
    while (1)
    {
        // Reestabelece conexão P2P se necessário
        if (socket_p2p == -1 && socket_escuta_p2p == -1)
        {
            inicializar_link_p2p(ip_peer_alvo, porta_p2p_comum,
                                 &socket_p2p, &socket_escuta_p2p,
                                 &conjunto_principal, &fd_maximo);
        }

        conjunto_leitura = conjunto_principal;

        // Espera por atividade em algum socket
        if (select(fd_maximo + 1, &conjunto_leitura, NULL, NULL, NULL) == SOCKET_ERROR)
        {
            perror("Erro crítico no select");
            exit(EXIT_FAILURE);
        }

        // Verifica todos os sockets por atividade
        for (int i = 0; i <= fd_maximo; i++)
        {
            if (!FD_ISSET(i, &conjunto_leitura))
                continue;

            // Tratamento de entrada do usuário via terminal
            if (i == STDIN_FILENO)
            {
                char buffer_comando[MAX_MSG_SIZE];
                if (fgets(buffer_comando, sizeof(buffer_comando), stdin) == NULL)
                    continue;

                // Remove nova linha
                buffer_comando[strcspn(buffer_comando, "\n")] = 0;

                if (strcmp(buffer_comando, "kill") == 0)
                {
                    if (socket_p2p != -1)
                    {
                        printf("[SERVER] Encerrando conexão P2P...\n");
                        char buffer_envio[MAX_MSG_SIZE];

                        construir_mensagem(buffer_envio, MAX_MSG_SIZE, REQ_DISCPEER, NULL, NULL);

                        send(socket_p2p, buffer_envio, strlen(buffer_envio), 0);
                    }
                    exit(0);
                }
                else
                {
                    printf("[SERVER] Comando não reconhecido: \"%s\"\n", buffer_comando);
                }
                continue;
            }

            // Nova conexão de cliente
            if (i == socket_escuta)
            {
                struct sockaddr_in endereco_cliente;
                socklen_t tamanho_endereco = sizeof(endereco_cliente);
                int socket_cliente;

                // Aceita nova conexão
                if ((socket_cliente = accept(socket_escuta, (struct sockaddr *)&endereco_cliente, &tamanho_endereco)) == SOCKET_ERROR)
                {
                    perror("[SERVER] Erro ao aceitar novo cliente");
                }
                else
                {
                    // Adiciona o novo socket ao conjunto principal
                    FD_SET(socket_cliente, &conjunto_principal);
                    if (socket_cliente > fd_maximo)
                    {
                        fd_maximo = socket_cliente;
                    }

                    // Log de informações do cliente
                    char ip_cliente[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &endereco_cliente.sin_addr, ip_cliente, sizeof(ip_cliente));
                    printf("[SERVER] Novo sensor conectado - IP: %s, Socket: %d\n", ip_cliente, socket_cliente);

                    // Configurações adicionais recomendadas
                    int opcao = 1;
                    if (setsockopt(socket_cliente, SOL_SOCKET, SO_KEEPALIVE, &opcao, sizeof(opcao)) == SOCKET_ERROR)
                    {
                        perror("[SERVER] Erro ao configurar SO_KEEPALIVE");
                    }
                }
            }
            // Nova conexão P2P
            else if (socket_escuta_p2p != -1 && i == socket_escuta_p2p)
            {
                struct sockaddr_in endereco_peer;
                socklen_t tamanho_endereco = sizeof(endereco_peer);
                int socket_p2p_aceito;

                // Aceita a nova conexão P2P
                if ((socket_p2p_aceito = accept(i, (struct sockaddr *)&endereco_peer, &tamanho_endereco)) == SOCKET_ERROR)
                {
                    perror("[SERVER] Erro ao aceitar conexão P2P");
                    // return;
                }
                else
                {
                    // Obtém informações do peer para logging
                    char ip_peer[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &endereco_peer.sin_addr, ip_peer, sizeof(ip_peer));
                    printf("[SERVER] Tentativa de conexão P2P de %s\n", ip_peer);

                    // Verifica se já existe uma conexão P2P ativa
                    if (socket_p2p != -1)
                    {
                        printf("[SERVER] Conexão P2P recusada - já existe uma conexão ativa\n");

                        char buffer_envio[MAX_MSG_SIZE];
                        construir_mensagem(buffer_envio, sizeof(buffer_envio), MSG_ERROR, "1", "Limite de peers excedido");
                        send(socket_p2p_aceito, buffer_envio, strlen(buffer_envio), 0);
                        close(socket_p2p_aceito);
                        // return;
                    }
                    else
                    {
                        // Configura a nova conexão P2P
                        socket_p2p = socket_p2p_aceito;
                        FD_SET(socket_p2p, &conjunto_principal);

                        // Atualiza o maior descritor se necessário
                        if (socket_p2p > fd_maximo)
                        {
                            fd_maximo = socket_p2p;
                        }

                        // Encerra o socket de escuta P2P
                        FD_CLR(socket_escuta_p2p, &conjunto_principal);
                        close(socket_escuta_p2p);
                        socket_escuta_p2p = -1;

                        printf("[SERVER] Conexão P2P estabelecida com %s (socket %d)\n", ip_peer, socket_p2p);
                    }
                }
            }
            // Comunicação P2P existente
            else if (socket_p2p != -1 && i == socket_p2p)
            {
                char buffer_p2p[MAX_MSG_SIZE];
                memset(buffer_p2p, 0, MAX_MSG_SIZE);
                ssize_t bytes_recebidos;

                if ((bytes_recebidos = recv(i, buffer_p2p, MAX_MSG_SIZE - 1, 0)) <= 0)
                {
                    // Tratamento de desconexão
                    if (handshake_p2p_completo == 0 && bytes_recebidos == 0)
                    {
                        printf("[P2P] Aviso: recv() retornou 0 durante handshake. Mantendo conexão.\n");
                    }
                    else if (bytes_recebidos == 0)
                    {
                        printf("[P2P] Conexão encerrada pelo peer (socket %d)\n", i);
                    }
                    else
                    {
                        perror("[P2P] Erro ao receber dados");
                    }

                    // Limpeza de recursos
                    close(i);
                    FD_CLR(i, &conjunto_principal);
                    socket_p2p = -1;
                    handshake_p2p_completo = 0;

                    // return;
                }
                else
                {
                    // Processamento da mensagem recebida
                    buffer_p2p[bytes_recebidos] = '\0';
                    processar_mensagem_recebida(i, buffer_p2p,
                                                &handshake_p2p_completo, &conjunto_principal, &socket_p2p,
                                                sensores_conectados, &contador_sensores,
                                                pedidos_pendentes);
                }
            }
            // Comunicação com cliente existente
            else
            {
                char buffer_recebido[MAX_MSG_SIZE];
                memset(buffer_recebido, 0, MAX_MSG_SIZE);
                ssize_t bytes_recebidos;

                if ((bytes_recebidos = recv(i, buffer_recebido, MAX_MSG_SIZE - 1, 0)) <= 0)
                {
                    // Trata desconexão do cliente
                    SensorInfo *sensor = NULL;

                    for (int i = 0; i < contador_sensores; i++)
                    {
                        if (sensores_conectados[i].socket_fd == i)
                        {
                            sensor = &sensores_conectados[i];
                        }
                    }

                    if (sensor != NULL)
                    {
                        sensor->is_active = 0;
                        contador_sensores--;
                        printf("[SERVER] Sensor desconectado: %s\n", sensor->sensor_id_str);
                    }

                    close(i);
                    FD_CLR(i, &conjunto_principal);
                }
                else
                {
                    // Processa mensagem recebida
                    buffer_recebido[bytes_recebidos] = '\0';

                    int codigo_recebido;
                    char payload1[256], payload2[256];
                    ParseResultType resultado = analisar_mensagem(buffer_recebido, &codigo_recebido, payload1, payload2);

                    if (resultado == PARSE_ERROR_INVALID_FORMAT)
                    {
                        fprintf(stderr, "[SERVER] Erro no formato da mensagem do cliente %d\n", i);
                    }
                    else
                    {
                        // Encaminha para processamento central
                        processar_mensagem_recebida(i, buffer_recebido, NULL,
                                                    &conjunto_principal, &socket_p2p,
                                                    sensores_conectados, &contador_sensores,
                                                    pedidos_pendentes);
                    }
                }
            }
        }
    }

    return 0;
}