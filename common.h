#ifndef COMMON_H
#define COMMON_H

#include <sys/select.h>

// Max message size
#define MAX_MSG_SIZE 500

//
#define TAMANHO_ID_SENSOR 10

// Server Limits
#define MAX_P2P_CONNECTIONS 1
#define MAX_CLIENTS 15

// --- MESSAGE CODES ---

// Mensagens de Controle
#define REQ_CONNPEER 20
#define RES_CONNPEER 21
#define REQ_DISCPEER 22
#define REQ_CONNSEN 23
#define RES_CONNSEN 24
#define REQ_DISCSEN 25

// Mensagens de Dados
#define REQ_CHECKALERT 36
#define RES_CHECKALERT 37
#define REQ_SENSLOC 38
#define RES_SENSLOC 39
#define REQ_SENSSTATUS 40 // Para SS
#define REQ_LOCLIST 40    // Para SL
#define RES_SENSSTATUS 41 // DE SS (payload: LocID)
#define RES_LOCLIST 41    // DE SL (payload: SenIDs)

// Mensagens de Erro ou Confirmação
#define MSG_OK 0
#define MSG_ERROR 255

// Códigos de Erro
#define ERR_PEER_LIMIT_EXCEEDED 1   // "01"
#define ERR_PEER_NOT_FOUND 2        // "02"
#define ERR_SENSOR_LIMIT_EXCEEDED 9 // "09"
#define ERR_SENSOR_NOT_FOUND 10     // "10"

// Códigos de Confirmação
#define OK_SUCCESSFUL_DISCONNECT 1 // "01"
#define OK_SUCCESSFUL_CREATE 2     // "02"
#define OK_SUCCESSFUL_UPDATE 3     // "03"

// Localização de area
#define AREA_NORTE_MIN 1
#define AREA_NORTE_MAX 3
#define AREA_SUL_MIN 4
#define AREA_SUL_MAX 5
#define AREA__LESTE_MIN 6
#define AREA__LESTE_MAX 7
#define AREA_OESTE_MIN 8
#define AREA_OESTE_MAX 9

#define SOCKET_ERROR -1

typedef struct
{
    int socket_fd;
    char sensor_id_str[TAMANHO_ID_SENSOR + 1];
    int location_id;
    int risk_status;
    int is_active;
} SensorInfo;

typedef struct
{
    int is_active;               // 1 se estiver em uso
    int original_client_fd;      // iniciou o 'check failure'.
    char sensor_id_in_query[20]; // O ID do sensor que se perguntou.
} PendingRequest;

typedef enum
{
    PARSE_ERROR_INVALID_FORMAT = -1,
    PARSE_SUCCESS_CODE_ONLY = 1,
    PARSE_SUCCESS_ONE_PAYLOAD = 2,
    PARSE_SUCCESS_TWO_PAYLOADS = 3
} ParseResultType;

typedef enum
{
    TIPO_SERVIDOR_STATUS,
    TIPO_SERVIDOR_LOCALIZACAO
} TipoServidor;

typedef struct
{
    int socket_sl;
    int socket_ss;

    char id_sensor_sl[20];
    char id_sensor_ss[20];
    int registrado_sl;
    int registrado_ss;

    char buffer_envio[MAX_MSG_SIZE];
    char buffer_recebido[MAX_MSG_SIZE];
} EstadoSensor;

extern TipoServidor tipo_servidor;

/**
 * @brief Cria e configura um socket para escuta de conexões
 *
 * @param porta Porta TCP para escuta
 * @param backlog Número máximo de conexões pendentes
 * @return int Descritor do socket ou SOCKET_ERROR em caso de falha
 */
int criar_e_configurar_socket_escuta(int porta, int backlog);

/**
 * @brief Inicializa ou reconfigura a conexão P2P entre servidores
 *
 * @param ip_peer_alvo IP do peer para conexão ativa
 * @param porta_p2p Porta para comunicação P2P
 * @param socket_p2p_ptr Ponteiro para o socket P2P ativo
 * @param socket_escuta_p2p_ptr Ponteiro para o socket de escuta P2P
 * @param conjunto_principal_ptr Ponteiro para o conjunto de descritores
 * @param fd_maximo_ptr Ponteiro para o maior descritor no conjunto
 */
void inicializar_link_p2p(const char *ip_peer_alvo, int porta_p2p,
                          int *socket_p2p_ptr, int *socket_escuta_p2p_ptr,
                          fd_set *conjunto_principal_ptr, int *fd_maximo_ptr);

/**
 * @brief Constroi uma mensagem formatada para comunicação entre componentes
 *
 * @param buffer_destino Buffer de destino onde a mensagem será construída
 * @param tamanho_buffer Tamanho máximo do buffer para prevenir overflow
 * @param codigo Código numérico da mensagem (usar constantes MSG_*)
 * @param payload1 Primeiro payload opcional (string)
 * @param payload2 Segundo payload opcional (string)
 *
 * @note A função garante que o buffer será sempre terminado com \0
 * @note Emite aviso se a mensagem for truncada por limite de tamanho
 */
void construir_mensagem(char *buffer_destino, size_t tamanho_buffer, int codigo,
                        const char *payload1, const char *payload2);

/**
 * @brief Aceita uma nova conexão de cliente e a adiciona ao conjunto principal
 *
 * @param socket_escuta Descritor do socket principal de escuta
 * @param conjunto_principal_ptr Ponteiro para o conjunto de descritores
 * @param fd_maximo_ptr Ponteiro para o maior descritor no conjunto
 *
 * @note Imprime informações de log sobre o novo cliente
 * @note Atualiza o conjunto de descritores e o valor máximo
 */
void tratar_nova_conexao_cliente(int socket_escuta, fd_set *conjunto_principal_ptr, int *fd_maximo_ptr);

/**
 * @brief Trata uma nova conexão P2P recebida no socket de escuta
 *
 * @param socket_escuta_p2p Socket de escuta P2P que recebeu a conexão
 * @param socket_p2p_ptr Ponteiro para o socket de comunicação P2P principal
 * @param socket_escuta_p2p_ptr Ponteiro para o socket de escuta P2P principal
 * @param conjunto_principal_ptr Ponteiro para o conjunto principal de descritores
 * @param fd_maximo_ptr Ponteiro para o maior descritor no conjunto
 *
 * @note Mantém apenas uma conexão P2P ativa por vez
 * @note Fecha o socket de escuta após aceitar conexão
 * @note Rejeita novas conexões se já houver uma ativa
 */
void tratar_conexao_p2p_entrante(int socket_escuta_p2p, int *socket_p2p_ptr,
                                 int *socket_escuta_p2p_ptr,
                                 fd_set *conjunto_principal_ptr, int *fd_maximo_ptr);

/**
 * @brief Processa dados recebidos ou desconexão em um socket P2P estabelecido
 *
 * @param socket_p2p_atual Socket P2P que teve atividade
 * @param socket_p2p_ptr Ponteiro para o socket P2P principal
 * @param handshake_completo_ptr Ponteiro para flag de handshake completo
 * @param conjunto_principal_ptr Ponteiro para conjunto principal de descritores
 * @param sensores Array de sensores conectados
 * @param contador_sensores_ptr Ponteiro para contador de sensores
 * @param pedidos_pendentes Array de pedidos pendentes
 *
 * @note Gerencia desconexões P2P e encaminha mensagens recebidas
 * @note Reinicia estado do handshake em caso de desconexão
 */
void tratar_comunicacao_p2p(int socket_p2p_atual, int *socket_p2p_ptr,
                            int *handshake_completo_ptr, fd_set *conjunto_principal_ptr,
                            SensorInfo *sensores, int *contador_sensores_ptr,
                            PendingRequest *pedidos_pendentes);

/**
 * @brief Processa mensagens recebidas de clientes ou peers (cérebro do protocolo)
 *
 * @param socket_origem Socket de origem da mensagem
 * @param buffer_recebido Mensagem recebida (string)
 * @param handshake_completo_ptr Ponteiro para flag de handshake P2P (NULL para clientes)
 * @param conjunto_principal_ptr Ponteiro para conjunto principal de sockets
 * @param socket_p2p_ptr Ponteiro para socket P2P
 * @param sensores Array de sensores conectados
 * @param contador_sensores_ptr Ponteiro para contador de sensores
 * @param proximo_id_sensor_ptr Ponteiro para próximo ID de sensor
 * @param pedidos_pendentes Array de pedidos pendentes
 */
void processar_mensagem_recebida(int socket_origem, const char *buffer_recebido,
                                 int *handshake_completo_ptr, fd_set *conjunto_principal_ptr,
                                 int *socket_p2p_ptr, SensorInfo *sensores,
                                 int *contador_sensores_ptr,
                                 PendingRequest *pedidos_pendentes);

/**
 * @brief Analisa uma mensagem recebida e extrai seu código e payloads
 *
 * @param buffer_entrada Mensagem recebida (string)
 * @param codigo_destino Ponteiro para armazenar o código da mensagem
 * @param payload1_destino Buffer para o primeiro payload (pode ser NULL)
 * @param payload2_destino Buffer para o segundo payload (pode ser NULL)
 * @return ParseResultType Resultado da análise (número de campos lidos ou erro)
 *
 * @note Limpa os buffers de destino antes de preenchê-los
 * @note Suporta mensagens com 0, 1 ou 2 payloads
 */
ParseResultType analisar_mensagem(const char *buffer_entrada, int *codigo_destino,
                                  char *payload1_destino, char *payload2_destino);

/**
 * @brief Trata o erro de limite máximo de sensores conectados
 *
 * @param socket_cliente Socket do cliente que excedeu o limite
 * @param buffer_envio Buffer para construção da mensagem de resposta
 * @param conjunto_principal_ptr Ponteiro para o conjunto principal de sockets
 *
 * @note Envia mensagem de erro e fecha a conexão com o cliente
 */
void tratar_erro_limite_clientes(int socket_cliente, char buffer_envio[MAX_MSG_SIZE],
                                 fd_set *conjunto_principal_ptr);

/**
 * @brief Registra um novo sensor no array de sensores conectados
 *
 * @param sensores Array de sensores
 * @param socket_cliente Socket do novo sensor
 * @param localizacao String com ID da localização
 * @param proximo_id_ptr Ponteiro para o próximo ID de sensor disponível
 * @param contador_sensores_ptr Ponteiro para o contador de sensores
 * @param buffer_envio Buffer para envio de mensagens
 *
 * @note Gera um ID único, status de risco aleatório e envia confirmação
 * @warning Caso não encontre slot vazio, indica erro crítico de sincronização
 */
void registrar_novo_sensor(SensorInfo *sensores, int socket_cliente, char localizacao[256],
                           int *contador_sensores_ptr,
                           char buffer_envio[MAX_MSG_SIZE]);

/**
 * @brief Trata uma requisição de desconexão de sensor
 *
 * @param sensores Array de sensores conectados
 * @param id_sensor ID do sensor a ser desconectado
 * @param buffer_envio Buffer para construção da mensagem de resposta
 * @param socket_cliente Socket do cliente que solicitou a desconexão
 * @param contador_sensores_ptr Ponteiro para o contador de sensores
 * @param conjunto_principal_ptr Ponteiro para o conjunto principal de sockets
 *
 * @note Verifica se o sensor existe, envia confirmação e limpa os recursos
 */
void tratar_desconexao_sensor(SensorInfo *sensores, char id_sensor[256],
                              char buffer_envio[MAX_MSG_SIZE], int socket_cliente,
                              int *contador_sensores_ptr, fd_set *conjunto_principal_ptr);

/**
 * @brief Processa requisição de localização de sensor (SL)
 *
 * @param sensores Array de sensores conectados
 * @param id_sensor ID do sensor a ser localizado
 * @param buffer_envio Buffer para construção da resposta
 * @param socket_cliente Socket do cliente que solicitou
 *
 * @note Responde com a localização do sensor ou erro se não encontrado
 */
void tratar_metodo_localizar(SensorInfo *sensores, char id_sensor[256],
                             char buffer_envio[MAX_MSG_SIZE], int socket_cliente);

/**
 * @brief Processa comando de verificação de falha (SS)
 *
 * @param socket_cliente Socket do cliente que solicitou
 * @param id_sensor ID do sensor a verificar
 * @param sensores Array de sensores conectados
 * @param socket_p2p_ptr Ponteiro para socket P2P
 * @param pedidos_pendentes Array de pedidos pendentes
 *
 * @note Verifica status do sensor e coordena com SL se necessário
 */
void tratar_comando_verificar_falha(int socket_cliente, char id_sensor[256],
                                    SensorInfo *sensores, int *socket_p2p_ptr,
                                    PendingRequest *pedidos_pendentes);

/**
 * @brief Processa comando de diagnóstico (SL) - lista sensores em uma localização
 *
 * @param localizacao Localização a ser verificada (string)
 * @param sensores Array de sensores conectados
 * @param socket_cliente Socket do cliente que solicitou
 * @param buffer_envio Buffer para construção da resposta
 *
 * @note Retorna lista de sensores na localização ou erro se não encontrado
 */
void tratar_comando_diagnosticar(char localizacao[256], SensorInfo *sensores,
                                 int socket_cliente, char buffer_envio[MAX_MSG_SIZE]);

/**
 * @brief Processa requisição de verificação de alerta (SL)
 *
 * @param sensores Array de sensores conectados
 * @param id_sensor ID do sensor a verificar
 * @param socket_cliente Socket do cliente (SS) que solicitou
 * @param buffer_envio Buffer para construção da resposta
 *
 * @note Responde com a localização do sensor ou erro se não encontrado
 */

void tratar_pedido_verificar_alerta(SensorInfo *sensores, char id_sensor[256],
                                    int socket_cliente, char buffer_envio[MAX_MSG_SIZE]);

/**
 * @brief Processa resposta de verificação de alerta (SS)
 *
 * @param localizacao Localização recebida do SL
 * @param sensores Array de sensores conectados (não utilizado nesta versão)
 * @param buffer_envio Buffer para construção de mensagens
 * @param pedidos_pendentes Array de pedidos pendentes
 *
 * @note Encaminha a resposta para o cliente original que fez a requisição
 */
void tratar_resposta_verificar_alerta(const char *localizacao, char *buffer_envio,
                                      PendingRequest *pedidos_pendentes);

/**
 * @brief Processa comunicação com um cliente (recebimento de dados ou desconexão)
 *
 * @param socket_cliente Socket do cliente com atividade
 * @param conjunto_principal_ptr Ponteiro para o conjunto principal de descritores
 * @param sensores Array de sensores conectados
 * @param contador_sensores_ptr Ponteiro para o contador de sensores
 * @param proximo_id_ptr Ponteiro para o próximo ID de sensor
 * @param socket_p2p_ptr Ponteiro para o socket P2P
 * @param pedidos_pendentes Array de pedidos pendentes
 *
 * @note Gerencia desconexões e encaminha mensagens recebidas para processamento
 */
void tratar_comunicacao_cliente(int socket_cliente, fd_set *conjunto_principal_ptr,
                                SensorInfo *sensores, int *contador_sensores_ptr,
                                int *socket_p2p_ptr,
                                PendingRequest *pedidos_pendentes);

/**
 * @brief Busca um sensor pelo ID na lista de sensores conectados
 *
 * @param sensores Array de sensores
 * @param id_sensor ID do sensor a ser buscado (string)
 * @return Ponteiro para o sensor encontrado ou NULL se não existir
 *
 * @note A busca é feita apenas entre sensores ativos (is_active == 1)
 */
SensorInfo *buscar_sensor_por_id(SensorInfo *sensores, const char *id_sensor);

/**
 * @brief Busca um sensor pelo descritor de socket
 *
 * @param sensores Array de sensores conectados
 * @param quantidade_sensores Número atual de sensores ativos
 * @param socket_fd Descritor de socket a ser buscado
 * @return Ponteiro para o sensor encontrado ou NULL se não existir
 *
 * @note A busca é feita apenas entre os sensores ativos (considerando sensor_count)
 */
SensorInfo *buscar_sensor_por_socket(SensorInfo *sensores, int quantidade_sensores, int socket_fd);

/**
 * @brief Estabelece conexão com um servidor
 *
 * @param ip_servidor Endereço IP do servidor
 * @param porta Porta do servidor
 * @return int Descritor do socket conectado ou SOCKET_ERROR em caso de falha
 *
 * @note Cria um socket TCP e conecta ao servidor especificado
 * @note Fecha o socket em caso de erro
 */
int conectar_servidor(const char *ip_servidor, int porta);

/**
 * @brief Processa entrada do teclado para o sensor
 *
 * @param estado Ponteiro para o estado atual do sensor
 *
 * @note Implementa os comandos:
 *   - kill: Encerra conexão com servidores
 *   - check failure: Verifica status de falha
 *   - locate <id>: Localiza um sensor específico
 *   - diagnose <loc>: Diagnostica sensores em uma localização
 */
void processar_entrada_teclado(EstadoSensor *estado);

/**
 * @brief Processa mensagens recebidas dos servidores
 *
 * @param socket_servidor Socket do servidor que enviou a mensagem
 * @param estado Ponteiro para o estado atual do sensor
 *
 * @note Trata todos os tipos de mensagens definidas no protocolo
 * @note Atualiza o estado do sensor conforme as mensagens recebidas
 */
void processar_mensagem_servidor(int socket_servidor, EstadoSensor *estado);

/**
 * @brief Analisa uma mensagem do protocolo e extrai seus componentes
 *
 * @param buffer Mensagem a ser analisada
 * @param codigo Ponteiro para armazenar o código da mensagem
 * @param payload1 Buffer para o primeiro payload (pode ser NULL)
 * @param payload2 Buffer para o segundo payload (pode ser NULL)
 * @return ParseResultType Resultado da análise (sucesso ou tipo de erro)
 *
 * @note Formato esperado: "<código> [payload1] [payload2]"
 * @note Limpa os buffers de payload antes do uso
 */
ParseResultType analisar_mensagem(const char *buffer, int *codigo,
                                  char *payload1, char *payload2);

/**
 * @brief Obtém o nome da região com base no ID de localização
 *
 * @param id_localizacao ID numérico da localização
 * @return const char* Nome da região correspondente
 *
 * @note Mapeamento de regiões:
 *   1-3: Norte
 *   4-5: Sul
 *   6-7: Leste
 *   8-10: Oeste
 *   outros: Área não coberta
 */
const char *obter_nome_regiao(int id_localizacao);

#endif // COMMON_H