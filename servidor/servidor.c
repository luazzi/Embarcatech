#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <string.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib") // Linka a biblioteca Winsock

#define SERVER_ADDR "0.0.0.0"
#define BUF_SIZE 2048
#define CONNECTION_TIMEOUT_SEC 10 // Tempo máximo de espera por uma conexão (10 segundos)

// Estrutura para armazenar os itens do cardápio
typedef struct {
    char nome[50];
    float preco;
} ItemCardapio;

// Itens do cardápio
ItemCardapio cardapio[] = {
    {"Pingado", 5.0},
    {"Coxinha", 4.5},
    {"Pão na Chapa", 6.0}
};
int num_itens = sizeof(cardapio) / sizeof(cardapio[0]);

// Função para exibir o cardápio
void exibir_cardapio() {
    printf("\nCardápio:\n");
    for (int i = 0; i < num_itens; i++) {
        printf("%d. %s - R$ %.2f\n", i + 1, cardapio[i].nome, cardapio[i].preco);
    }
    printf("Digite o número do item e a quantidade (ex: 1 2): ");
}

// Função para salvar o pedido em um arquivo CSV
void salvar_pedido_csv(int comanda, int* itens, int* quantidades) {
    FILE* arquivo = fopen("pedidos.csv", "a"); // Abre o arquivo no modo "append" (adiciona ao final)
    if (arquivo == NULL) {
        printf("Erro ao abrir o arquivo pedidos.csv.\n");
        return;
    }

    // Escreve o cabeçalho se o arquivo estiver vazio
    if (ftell(arquivo) == 0) {
        fprintf(arquivo, "Comanda,Item,Quantidade,Valor Unitário,Subtotal\n");
    }

    // Escreve os itens do pedido
    for (int i = 0; i < num_itens; i++) {
        if (quantidades[i] > 0) {
            float subtotal = cardapio[i].preco * quantidades[i];
            fprintf(arquivo, "%d,%s,%d,%.2f,%.2f\n", comanda, cardapio[i].nome, quantidades[i], cardapio[i].preco, subtotal);
        }
    }

    fclose(arquivo);
    printf("Pedido salvo no arquivo pedidos.csv.\n");
}

// Função para fechar a comanda
void fechar_comanda(int comanda) {
    FILE* arquivo = fopen("pedidos.csv", "r");
    if (arquivo == NULL) {
        printf("Erro ao abrir o arquivo pedidos.csv.\n");
        return;
    }

    char linha[BUF_SIZE];
    float total = 0.0;
    int comanda_encontrada = 0;

    printf("\nItens da Comanda %d:\n", comanda);
    printf("Item\t\tQuantidade\tValor Unitário\tSubtotal\n");

    while (fgets(linha, BUF_SIZE, arquivo) != NULL) {
        int comanda_atual;
        char item[50];
        int quantidade;
        float valor_unitario, subtotal;

        if (sscanf(linha, "%d,%[^,],%d,%f,%f", &comanda_atual, item, &quantidade, &valor_unitario, &subtotal) == 5) {
            if (comanda_atual == comanda) {
                comanda_encontrada = 1;
                printf("%s\t\t%d\t\tR$ %.2f\t\tR$ %.2f\n", item, quantidade, valor_unitario, subtotal);
                total += subtotal;
            }
        }
    }

    fclose(arquivo);

    if (comanda_encontrada) {
        printf("Total da Comanda %d: R$ %.2f\n", comanda, total);
    } else {
        printf("Comanda %d não encontrada.\n", comanda);
    }

    // Remove os itens da comanda do arquivo CSV
    arquivo = fopen("pedidos.csv", "r");
    FILE* temp_arquivo = fopen("temp.csv", "w");

    if (arquivo == NULL || temp_arquivo == NULL) {
        printf("Erro ao processar o arquivo CSV.\n");
        return;
    }

    while (fgets(linha, BUF_SIZE, arquivo) != NULL) {
        int comanda_atual;
        if (sscanf(linha, "%d,", &comanda_atual) == 1 && comanda_atual != comanda) {
            fprintf(temp_arquivo, "%s", linha);
        }
    }

    fclose(arquivo);
    fclose(temp_arquivo);

    remove("pedidos.csv");
    rename("temp.csv", "pedidos.csv");

    printf("Comanda %d fechada.\n", comanda);
}

// Função para processar o pedido
void processar_pedido(SOCKET client_sock, int comanda) {
    int item, quantidade;
    float total = 0.0;
    char buffer[BUF_SIZE];
    int itens[num_itens]; // Armazena as quantidades de cada item
    memset(itens, 0, sizeof(itens)); // Inicializa todas as quantidades como 0

    while (1) {
        // Exibe o cardápio no terminal do servidor
        exibir_cardapio();

        // Lê a escolha do usuário no terminal do servidor
        if (fgets(buffer, BUF_SIZE, stdin) != NULL) {
            if (sscanf(buffer, "%d %d", &item, &quantidade) == 2) {
                if (item >= 1 && item <= num_itens) {
                    float subtotal = cardapio[item - 1].preco * quantidade;
                    total += subtotal;
                    itens[item - 1] += quantidade; // Atualiza a quantidade do item

                    // Exibe o item adicionado no terminal do servidor
                    printf("Adicionado: %d x %s - Subtotal: R$ %.2f\n",
                           quantidade, cardapio[item - 1].nome, subtotal);
                } else {
                    printf("Item inválido. Tente novamente.\n");
                }
            } else if (strcmp(buffer, "FINALIZAR\n") == 0) {
                // Finaliza o pedido
                printf("Pedido finalizado. Total: R$ %.2f\n", total);

                // Envia o comando FINALIZAR para o cliente (placa)
                snprintf(buffer, BUF_SIZE, "FINALIZAR");
                int bytes_sent = send(client_sock, buffer, strlen(buffer), 0);
                if (bytes_sent == SOCKET_ERROR) {
                    printf("Erro ao enviar comando FINALIZAR. Código: %d\n", WSAGetLastError());
                }

                // Salva o pedido no arquivo CSV
                salvar_pedido_csv(comanda, itens, itens);
                break;
            } else {
                printf("Entrada inválida. Tente novamente.\n");
            }
        }
    }
}

int main() {
    WSADATA wsa;
    SOCKET server_sock, client_sock;
    struct sockaddr_in server, client;
    int client_len = sizeof(client);
    char command[BUF_SIZE];
    int server_port; // Variável para armazenar a porta
    time_t start_time;

    // Loop principal para permitir a redefinição da porta
    while (1) {
        // Solicita ao usuário que digite a porta
        printf("Digite o número da comanda: ");
        scanf("%d", &server_port);
        getchar(); // Limpa o buffer do teclado após o scanf

        // Inicializa o Winsock
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            printf("Erro na inicialização do Winsock. Código: %d\n", WSAGetLastError());
            return 1;
        }

        // Cria o socket do servidor
        if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
            printf("Erro ao criar o socket. Código: %d\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Configuração do servidor
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        server.sin_port = htons(server_port); // Usa a porta digitada pelo usuário

        // Liga o socket ao endereço e porta
        if (bind(server_sock, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
            printf("Erro ao fazer bind. Código: %d\n", WSAGetLastError());
            closesocket(server_sock);
            WSACleanup();
            return 1;
        }

        // Coloca o socket para escutar conexões
        if (listen(server_sock, 1) == SOCKET_ERROR) {
            printf("Erro ao escutar. Código: %d\n", WSAGetLastError());
            closesocket(server_sock);
            WSACleanup();
            return 1;
        }

        printf("Servidor ouvindo em %s:%d\n", SERVER_ADDR, server_port);

        // Aguarda conexão do cliente com timeout
        printf("Aguardando conexão do cliente por %d segundos...\n", CONNECTION_TIMEOUT_SEC);
        start_time = time(NULL); // Marca o tempo inicial

        while (1) {

            if (time(NULL) - start_time >= CONNECTION_TIMEOUT_SEC) {
                break;
            }

            // Verifica se há uma conexão pendente
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(server_sock, &read_fds);

            struct timeval timeout;
            timeout.tv_sec = 1; // Timeout de 1 segundo para select
            timeout.tv_usec = 0;

            int select_result = select(0, &read_fds, NULL, NULL, &timeout);
            if (select_result == SOCKET_ERROR) {
                printf("Erro ao verificar conexão. Código: %d\n", WSAGetLastError());
                break;
            } else if (select_result > 0) {
                // Aceita a conexão do cliente
                client_sock = accept(server_sock, (struct sockaddr *)&client, &client_len);
                if (client_sock == INVALID_SOCKET) {
                    printf("Erro ao aceitar conexão. Código: %d\n", WSAGetLastError());
                    break;
                }

                printf("Cliente conectado. Enviando confirmação...\n");
                send(client_sock, "CONNECTED", strlen("CONNECTED"), 0);

                // Loop para enviar comandos
                while (1) {
                    printf("Digite um comando: ");
                    if (fgets(command, BUF_SIZE, stdin) != NULL) {
                        command[strcspn(command, "\n")] = 0; // Remove a nova linha

                        // Envia o comando para o cliente
                        int bytes_sent = send(client_sock, command, strlen(command), 0);
                        if (bytes_sent == SOCKET_ERROR) {
                            printf("Erro ao enviar comando. Código: %d\n", WSAGetLastError());
                            break;
                        }

                        printf("Enviado: %s (%d bytes)\n", command, bytes_sent);

                        // Verifica se o comando é PEDIDO
                        if (strcmp(command, "PEDIDO") == 0) {
                            processar_pedido(client_sock, server_port); // Processa o pedido no terminal do servidor
                        }

                        // Verifica se o comando é FECHAR_COMANDA
                        if (strcmp(command, "FECHAR_COMANDA") == 0) {
                            fechar_comanda(server_port); // Fecha a comanda
                            closesocket(client_sock);
                            break;
                        }

                        // Verifica se o comando é PEDIDO_PRONTO
                        if (strcmp(command, "PEDIDO_PRONTO") == 0) {
                            printf("Comando PEDIDO_PRONTO enviado. Desconectando cliente...\n");
                            closesocket(client_sock); // Fecha a conexão com o cliente
                            break; // Volta ao estado inicial
                        }

                        // Verifica se o comando é EXIT
                        if (strcmp(command, "EXIT") == 0) {
                            printf("Encerrando servidor...\n");
                            closesocket(client_sock);
                            closesocket(server_sock);
                            WSACleanup();
                            return 0;
                        }
                    } else {
                        printf("Erro ao ler entrada do usuário.\n");
                        break;
                    }
                }

                // Fecha o socket do cliente
                closesocket(client_sock);
                printf("Cliente desconectado.\n");
            }
        }

        // Fecha o socket do servidor e limpa Winsock
        closesocket(server_sock);
        WSACleanup();

        // Pergunta ao usuário se deseja tentar outra porta
        printf("Deseja tentar outra porta? (s/n): ");
        char resposta;
        scanf(" %c", &resposta);
        getchar(); // Limpa o buffer do teclado

        if (resposta != 's' && resposta != 'S') {
            printf("Encerrando servidor...\n");
            break;
        }
    }

    return 0;
}