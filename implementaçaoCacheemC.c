// SIMULADOR DE CACHE
// ARQUITETURA E ORGANIZAÇÃO DE COMPUTADORES II
// Larissa Gabriela

#include <stdio.h>    
#include <stdlib.h> 
#include <stdint.h>   // Biblioteca para tipos inteiros fixos (uint32_t, uint64_t)
#include <time.h>     // Biblioteca para gerar números aleatórios (rand, srand)

// ========================================================
// Função: ntohl (Network to Host Long)
// Descrição: Converte um número de 32 bits da ordem de bytes
//            "little-endian" (Intel) para "big-endian".
//            Necessário porque fread lê os bytes na ordem original do arquivo.
// ========================================================
static uint32_t ntohl(uint32_t x) {
    return ((x & 0xFF) << 24) |         // move o byte menos significativo para o mais significativo
           ((x & 0xFF00) << 8) |        // move o 2º byte para a posição correta
           ((x & 0xFF0000) >> 8) |      // move o 3º byte para a posição correta
           ((x & 0xFF000000) >> 24);    // move o byte mais significativo para o menos significativo
}

// ========================================================
// Constantes para segurança
// Limita o tamanho máximo da associatividade e número de conjuntos
#define MAX_ASSOC 32
#define MAX_SETS 8192

// ========================================================
// Estrutura CacheBlock
// Representa um bloco individual da cache
typedef struct {
    int valid;             // 1 se o bloco é válido, 0 se está vazio
    uint32_t tag;          // Tag do endereço armazenado
    int lru_counter;       // Contador para política LRU/FIFO
} CacheBlock;

// Ponteiro para a matriz 2D de blocos da cache
CacheBlock **cache;

// ========================================================
// Estrutura VisitSet
// Guarda todas as (index, tag) já acessadas para classificar os misses
typedef struct {
    uint32_t *tags;    // Vetor dinâmico que armazena tags visitadas
    int size;          // Número atual de tags armazenadas
    int capacity;      // Capacidade máxima antes de expandir
} VisitSet;

VisitSet set_visitado;  // Instância global para rastrear os acessos

// ========================================================
// Variáveis globais para estatísticas
int total_acessos = 0;         // Total de acessos à cache
int hits = 0;                  // Número de hits
int miss_total = 0;            // Número total de misses
int miss_compulsorio = 0;      // Misses compulsórios
int miss_conflito = 0;         // Misses por conflito
int miss_capacidade = 0;       // Misses por capacidade
int blocos_validos = 0;        // Quantidade de blocos válidos na cache

// ========================================================
// Protótipos de funções
int is_potencia2(int x);  // Verifica se número é potência de 2
int log2int(int x);       // Calcula log base 2 (inteiro)
void inicializar_visitado(); // Inicializa o vetor de tags visitadas
void liberar_visitado();     // Libera memória alocada pelo VisitSet
void inicializar_cache(int nsets, int assoc); // Cria e zera a cache
FILE* processar_arquivo(char *f); // Abre arquivo de entrada
void simular_acesso_cache(uint32_t _endereco, int nsets, int bsize, int assoc, char *sub); // Simula o acesso à cache
void imprimir_estatisticas(int flag, int flag_out); // Imprime estatísticas finais

// ========================================================
// Corpo das funções
// ========================================================

// --------------------------------------------------------
// Função: is_potencia2
// Descrição: verifica se um número é potência de 2
// --------------------------------------------------------
int is_potencia2(int x) {
    if (x <= 0) return 0;          // Número <=0 não é potência de 2
    while (x > 1) {
        if (x % 2 != 0) return 0;  // Se não é divisível por 2, não é potência de 2
        x = x / 2;                 // Divide por 2
    }
    return 1;                      // Se chegou a 1, é potência de 2
}

// --------------------------------------------------------
// Função: log2int
// Descrição: retorna quantos bits são necessários para representar o número
// Exemplo: log2(8) = 3 porque 2^3 = 8
// --------------------------------------------------------
int log2int(int x) {
    int resultado_log = 0;
    while (x > 1) {
        x = x / 2;
        resultado_log++;           // Conta quantas divisões por 2 foram feitas
    }
    return resultado_log;
}

// --------------------------------------------------------
// Função: inicializar_visitado
// Descrição: cria vetor para armazenar as tags já visitadas
// --------------------------------------------------------
void inicializar_visitado() {
    set_visitado.tags = malloc(100 * sizeof(uint32_t)); // Aloca espaço inicial para 100 elementos
    set_visitado.capacity = 100;                        // Define capacidade inicial
    set_visitado.size = 0;                               // Nenhuma tag armazenada no início
}

// --------------------------------------------------------
// Função: liberar_visitado
// Descrição: libera memória usada pelo VisitSet
// --------------------------------------------------------
void liberar_visitado() {
    free(set_visitado.tags);
}

// --------------------------------------------------------
// Função: inicializar_cache
// Descrição: cria uma matriz 2D representando os conjuntos e as vias da cache
// --------------------------------------------------------
void inicializar_cache(int nsets, int assoc) {
    cache = malloc(nsets * sizeof(CacheBlock*));      // Aloca vetor de linhas
    for (int i = 0; i < nsets; i++) {
        cache[i] = malloc(assoc * sizeof(CacheBlock)); // Aloca vetor de colunas (vias) para cada linha
        for (int j = 0; j < assoc; j++) {
            cache[i][j] = (CacheBlock){0, 0, 0};      // Inicializa blocos como inválidos
        }
    }
    blocos_validos = 0;                               // Nenhum bloco está válido inicialmente
}

// --------------------------------------------------------
// Função: processar_arquivo
// Descrição: abre o arquivo binário de entrada
// --------------------------------------------------------
FILE *processar_arquivo(char *arquivo_de_entrada) {
    FILE *fp = fopen(arquivo_de_entrada, "rb");       // Abre arquivo para leitura binária
    if (!fp) {
        perror("Erro ao abrir o arquivo");            // Mostra erro no terminal
        return NULL;                                  // Retorna NULL se falhou
    }
    return fp;
}

// --------------------------------------------------------
// Função: simular_acesso_cache
// Descrição: faz o processamento do endereço e simula o acesso à cache
// --------------------------------------------------------
void simular_acesso_cache(uint32_t _endereco, int nsets, int bsize, int assoc, char *substituicao) {
    total_acessos++;                                  // Incrementa total de acessos
    uint32_t endereco = ntohl(_endereco);             // Corrige ordem de bytes

    // Divide o endereço em offset, índice e tag
    int offset_bits = log2int(bsize);                 // Quantidade de bits para offset
    int index_bits  = log2int(nsets);                 // Quantidade de bits para índice
    uint32_t index  = (endereco >> offset_bits) & (nsets - 1); // Isola índice
    uint32_t tag    = endereco >> (offset_bits + index_bits);  // Isola tag

    // Verifica se é HIT
    int hit = 0;
    for (int i = 0; i < assoc; i++) {
        if (cache[index][i].valid && cache[index][i].tag == tag) {
            hits++;                                    // Incrementa hits
            hit = 1;
            if (substituicao[0] == 'L') {             // Atualiza LRU se necessário
                cache[index][i].lru_counter = total_acessos;
            }
            break;                                     // Não precisa verificar outras vias
        }
    }

    if (hit) return;                                  // Se foi HIT, não precisa fazer mais nada

    // Miss: incrementa contador geral
    miss_total++;

    // Chave única para saber se é a 1ª vez que a tag aparece
    uint64_t chave = ((uint64_t)index << 32) | tag;
    int novo = 1;

    // Verifica se a chave já estava no conjunto visitado
    for (int i = 0; i < set_visitado.size; i++) {
        if (set_visitado.tags[i] == chave) {
            novo = 0;                                  // Já visitado
            break;
        }
    }

    // Se for nova, adiciona ao vetor de visitados
    if (novo) {
        if (set_visitado.size == set_visitado.capacity) {
            set_visitado.capacity *= 2;                // Dobra capacidade do vetor
            set_visitado.tags = realloc(
                set_visitado.tags,
                set_visitado.capacity * sizeof(uint64_t)
            );
        }
        set_visitado.tags[set_visitado.size++] = chave;
    }

    // Verifica se o conjunto está cheio
    int set_cheio = 1;
    for (int i = 0; i < assoc; i++) {
        if (!cache[index][i].valid) {
            set_cheio = 0;                             // Existe via livre
            break;
        }
    }

    // Classifica tipo de miss
    if (novo && !set_cheio) {
        miss_compulsorio++;                             // Miss compulsório
    } else if (novo && blocos_validos < (long)nsets * assoc) {
        miss_conflito++;                                // Miss por conflito
    } else {
        miss_capacidade++;                              // Miss por capacidade
    }

    // Politica de substituição
    int via = -1;
    for (int i = 0; i < assoc; i++) {
        if (!cache[index][i].valid) {
            via = i;                                    // Encontrou via livre
            break;
        }
    }

    if (via < 0) {                                      // Precisa substituir
        if (substituicao[0] == 'R') {
            via = rand() % assoc;                       // Substituição aleatória
        } else {
            via = 0;
            for (int i = 1; i < assoc; i++) {
                if (cache[index][i].lru_counter < cache[index][via].lru_counter) {
                    via = i;                            // Escolhe bloco menos usado
                }
            }
        }
    }

    // Atualiza o bloco selecionado
    if (!cache[index][via].valid) blocos_validos++;     // Incrementa blocos válidos
    cache[index][via].valid = 1;                        // Marca como válido
    cache[index][via].tag = tag;                        // Atualiza tag
    cache[index][via].lru_counter = total_acessos;      // Atualiza contador LRU
}

// --------------------------------------------------------
// Função: imprimir_estatisticas
// Descrição: imprime os resultados (absolutos ou taxas)
// --------------------------------------------------------
void imprimir_estatisticas(int flag, int flag_out) {
    double t_hit = (double)hits / total_acessos;
    double t_miss = (double)miss_total / total_acessos;
    double t_compu, t_confl, t_capac;

    if (miss_total) {
        t_compu = (double)miss_compulsorio / miss_total;
        t_confl = (double)miss_conflito / miss_total;
        t_capac = (double)miss_capacidade / miss_total;
    } else {
        t_compu = t_confl = t_capac = 0;
    }

    if (flag == 0) { // imprime com opções
        if (flag_out == 0 || flag_out == 1) {
            printf("Total: %d\nHits: %d\nMisses: %d\nCompulsorios: %d\nConflito: %d\nCapacidade: %d\n",
                total_acessos, hits, miss_total, miss_compulsorio, miss_conflito, miss_capacidade);
        }
        if (flag_out == 0 || flag_out == 2) {
            printf("\n##### TAXAS #####\nHit: %.4f\nMiss: %.4f\nCompulsorios: %.4f\nConflito: %.4f\nCapacidade: %.4f\n",
                t_hit, t_miss, t_compu, t_confl, t_capac);
        }
    } else { // imprime versão compacta
        printf("%d %.4f %.4f %.4f %.4f %.4f\n",
            total_acessos, t_hit, t_miss, t_compu, t_confl, t_capac);
    }
}

// --------------------------------------------------------
// Função: main
// Descrição: ponto de entrada do programa
// --------------------------------------------------------
int main(int argc, char *argv[]) {
    if (argc != 7) { // Verifica número de argumentos
        printf("Número de argumentos incorreto.\nUse: ./cache_simulator.exe <nsets> <bsize> <assoc> <substituição> <flag_saida> arquivo_de_entrada\n");
        return 1;
    }

    srand(time(NULL)); // Inicializa gerador aleatório

    // Lê parâmetros da linha de comando
    int nsets = atoi(argv[1]);
    int bsize = atoi(argv[2]);
    int assoc = atoi(argv[3]);
    char *substituicao = argv[4];
    int flag_saida = atoi(argv[5]);
    char *arquivo_entrada = argv[6];

    // Valida potências de 2
    if (!is_potencia2(nsets) || !is_potencia2(bsize) || !is_potencia2(assoc)) {
        fprintf(stderr, "Erro: nsets, bsize e assoc devem ser potências de 2.\n");
        return 1;
    }

    FILE *f = processar_arquivo(arquivo_entrada); // Abre arquivo

    inicializar_cache(nsets, assoc);  // Prepara cache
    inicializar_visitado();           // Prepara vetor de tags

    uint32_t end;                     // Variável para armazenar endereços
    while (fread(&end, sizeof(end), 1, f) == 1) {
        simular_acesso_cache(end, nsets, bsize, assoc, substituicao);
    }

    fclose(f);                        // Fecha arquivo

    int flag_out = 3;                 // Valor padrão
    if (!flag_saida) {                // Permite usuário escolher saída
        printf("Exibir dados:\n[1] - Absolutos  [2] - Taxas  [0] - Ambos\n");
        scanf("%d", &flag_out);
    }

    imprimir_estatisticas(flag_saida, flag_out); // Imprime resultados

    liberar_visitado();               // Libera memória do VisitSet
    for (int i = 0; i < nsets; i++) free(cache[i]); // Libera cada linha
    free(cache);                      // Libera matriz da cache

    return 0;
}
