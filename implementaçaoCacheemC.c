// SIMULADOR DE CACHE
// ARQUITETURA E ORGANIZAÇÃO DE COMPUTADORES II

#include <stdio.h>   // biblioteca padrão para entrada/saída
#include <stdlib.h>  // biblioteca padrão para funções como malloc, free, atoi
#include <stdint.h>  // tipos inteiros com tamanhos fixos, como uint32_t
#include <time.h>    // para inicializar o gerador de números aleatórios

// função que converte endianness de um inteiro de 32 bits
// (para arquiteturas little-endian como Intel)
static uint32_t ntohl(uint32_t x) {
    // inverte os bytes de posição
    return ((x & 0xFF) << 24) |            // byte menos significativo vai para o mais significativo
           ((x & 0xFF00) << 8) |           // segundo byte menos significativo vai para o segundo mais significativo
           ((x & 0xFF0000) >> 8) |         // segundo byte mais significativo vai para o segundo menos significativo
           ((x & 0xFF000000) >> 24);       // byte mais significativo vai para o menos significativo
}

// Limites máximos para evitar estouro de memória
#define MAX_ASSOC 32     // máximo de associatividade
#define MAX_SETS 8192    // máximo de conjuntos

// Estrutura para representar um bloco na cache
typedef struct {
    int valid;             // 1 se o bloco está válido (ocupado), 0 se vazio
    uint32_t tag;          // tag do endereço (identifica o bloco)
    int lru_counter;       // contador usado na política LRU ou FIFO
} CacheBlock;

// Ponteiro para a cache simulada (matriz de conjuntos x associatividade)
CacheBlock **cache;

// Estrutura para rastrear os pares (index, tag) já acessados
typedef struct {
    uint32_t *tags;    // vetor com as tags já acessadas
    int size;          // tamanho atual (quantos elementos armazenados)
    int capacity;      // capacidade máxima antes de realocar memória
} VisitSet;

// Variável global que guarda o conjunto de tags visitadas
VisitSet set_visitado;

// Variáveis para estatísticas da simulação
long total_acessos = 0;         // total de acessos feitos
long hits = 0;                  // número de hits (acessos que estavam na cache)
long miss_compulsorio = 0;      // número de misses compulsórios
long miss_total = 0;            // número total de misses
long miss_conflito = 0;         // número de misses por conflito
long miss_capacidade = 0;       // número de misses por capacidade
long blocos_validos = 0;        // blocos válidos atualmente na cache

// Declaração (protótipos) das funções
int is_potencia2(int x);                  // verifica se número é potência de 2
int log2int(int x);                       // calcula log base 2 inteiro
void inicializar_visitado();              // inicializa a lista de tags visitadas
void liberar_visitado();                  // libera memória da lista visitada
void inicializar_cache(int nsets, int assoc); // inicializa a cache
FILE* processar_arquivo(char *f);         // abre arquivo binário
void simular_acesso_cache(uint32_t _endereco, int nsets, int bsize, int assoc, char *sub); // simula acesso
void imprimir_estatisticas(int flag, int flag_out); // imprime estatísticas

/////////////////////
// CORPO DAS FUNÇÕES
/////////////////////

// Verifica se um número é potência de 2 usando operação bit a bit
int is_potencia2(int x) {
    return x && !(x & (x - 1)); // exemplo: 8 (1000) & (0111) == 0
}

// Calcula log base 2 inteiro (conta quantas vezes divide por 2)
int log2int(int x) {
    int r=0;
    while (x > 1) {
        x >>= 1; // desloca bits para direita (divide por 2)
        r++;     // incrementa contador
    }
    return r;
}

// Inicializa o vetor de tags visitadas com capacidade inicial de 100
void inicializar_visitado(){
    set_visitado.tags = malloc(100 * sizeof(uint32_t)); // aloca espaço para 100 tags
    set_visitado.capacity = 100;  // capacidade inicial
    set_visitado.size = 0;        // ainda sem elementos
}

// Libera memória do vetor de tags visitadas
void liberar_visitado(){
    free(set_visitado.tags);
}

// Inicializa a cache como matriz (nsets linhas x assoc colunas)
void inicializar_cache(int nsets, int assoc) {
    cache = malloc(nsets * sizeof(CacheBlock*)); // aloca vetor de linhas

    for (int i = 0; i < nsets; i++) {
        cache[i] = malloc(assoc * sizeof(CacheBlock)); // aloca colunas em cada linha

        for (int j = 0; j < assoc; j++) {
            cache[i][j] = (CacheBlock){0, 0, 0}; // inicializa cada bloco como inválido
        }
    }
    blocos_validos = 0; // no início nenhum bloco está preenchido
}

// Abre o arquivo binário para leitura dos endereços
FILE *processar_arquivo(char *arquivo_de_entrada){
    FILE *fp = fopen(arquivo_de_entrada, "rb"); // abre em modo binário
    if (!fp){
        perror("Erro ao abrir o arquivo"); // erro se falhar
        return NULL;
    }
    return fp; // retorna ponteiro para arquivo
}

// Simula o acesso de um endereço à cache
void simular_acesso_cache(uint32_t _endereco, int nsets, int bsize, int assoc, char *substituicao) {
    total_acessos++; // incrementa total de acessos

    // Inverte os bits (endianess) para leitura correta
    uint32_t endereco = ntohl(_endereco);

    // Calcula quantos bits são usados para offset e índice
    int offset_bits = log2int(bsize);
    int index_bits  = log2int(nsets);
    // Extrai o índice removendo os bits de offset e isolando os de índice
    uint32_t index  = (endereco >> offset_bits) & (nsets - 1);
    // Extrai a tag removendo bits de offset e índice
    uint32_t tag    = endereco >> (offset_bits + index_bits);

    int hit = 0; // flag de hit

    // Procura o bloco com a tag na cache
    for (int i = 0; i < assoc; i++) {
        if (cache[index][i].valid && cache[index][i].tag == tag) {
            hits++; // encontrou (hit)
            hit = 1;

            // Atualiza contador LRU (último acesso)
            if (substituicao[0] == 'L') {
                cache[index][i].lru_counter = total_acessos;
            }
            break;
        }
    }

    if (hit) return; // se foi hit, encerra função

    // Caso contrário é miss
    miss_total++;
    uint64_t chave = ((uint64_t)index << 32) | tag; // cria chave única

    int novo = 1; // flag se é primeira vez que vê essa chave

    // Verifica se já foi acessado antes
    for (int i = 0; i < set_visitado.size; i++) {
        if (set_visitado.tags[i] == chave) {
            novo = 0;
            break;
        }
    }

    // Se for primeira vez, adiciona ao conjunto de visitados
    if (novo) {
        if (set_visitado.size == set_visitado.capacity) {
            set_visitado.capacity *= 2; // dobra capacidade
            set_visitado.tags = realloc(set_visitado.tags, set_visitado.capacity * sizeof(uint64_t));
        }
        set_visitado.tags[set_visitado.size++] = chave; // adiciona chave
    }

    // Verifica se o conjunto (linha da cache) está cheio
    int set_cheio = 1;
    for (int i = 0; i < assoc; i++) {
        if (!cache[index][i].valid) {
            set_cheio = 0;
            break;
        }
    }

    // Classifica o tipo de miss
    if (novo && !set_cheio) miss_compulsorio++; // primeiro acesso + espaço livre
    else if (novo && blocos_validos < (long)nsets*assoc) miss_conflito++;
    else if (novo) miss_capacidade++;
    else if (set_cheio && blocos_validos < (long)nsets*assoc) miss_conflito++;
    else miss_capacidade++;

    // Política de substituição: procura uma via
    int via = -1;

    // Tenta encontrar uma via livre
    for (int i = 0; i < assoc; i++) {
        if (!cache[index][i].valid) {
            via = i; // achou via livre
            break;
        }
    }

    // Se não achou via livre, aplica substituição
    if (via < 0) {
        if (substituicao[0] == 'R') {
            via = rand() % assoc; // substituição aleatória
        } else {
            // substituição LRU (menor contador)
            via = 0;
            for (int i = 1; i < assoc; i++) {
                if (cache[index][i].lru_counter < cache[index][via].lru_counter) {
                    via = i;
                }
            }
        }
    }

    // Marca bloco como válido e atualiza tag e contador
    if (!cache[index][via].valid) blocos_validos++;
    cache[index][via].valid = 1;
    cache[index][via].tag = tag;
    cache[index][via].lru_counter = total_acessos;
}

// Imprime estatísticas no terminal
void imprimir_estatisticas(int flag, int flag_out) {
    double t_hit = (double)hits / total_acessos;
    double t_miss = (double)miss_total / total_acessos;

    double t_compu = miss_total ? (double)miss_compulsorio / miss_total : 0;
    double t_confl = miss_total ? (double)miss_conflito / miss_total : 0;
    double t_capac = miss_total ? (double)miss_capacidade / miss_total : 0;

    if (flag == 0) {
        if (flag_out == 0 || flag_out == 1) {
            printf("Total: %ld\nhits: %ld\nmisses: %ld\ncompulsorios: %ld\nconflito: %ld\ncapacidade: %ld\n",
                total_acessos, hits, miss_total, miss_compulsorio, miss_conflito, miss_capacidade);
        }
        if (flag_out == 0 || flag_out == 2) {
            printf("##### TAXAS #####\nhit: %.4f\nmiss: %.4f\ncompulsorios: %.4f\nconflito: %.4f\ncapacidade: %.4f\n",
                t_hit, t_miss, t_compu, t_confl, t_capac);
        }
    } else {
        printf("%ld %.4f %.4f %.4f %.4f %.4f\n", total_acessos, t_hit, t_miss, t_compu, t_confl, t_capac);
    }
}

// Função principal
int main (int argc, char *argv[]) {
    // Confere número de argumentos
    if (argc != 7) {
        printf("Numero de argumentos incorreto. Utilize:\n");
        printf("./cache_simulator.exe <nsets> <bsize> <assoc> <substituição> <flag_saida> arquivo_de_entrada\n");
        return 1;
    }

    srand(time(NULL)); // inicializa gerador de números aleatórios

    // Lê parâmetros da linha de comando
    int nsets = atoi(argv[1]);
    int bsize = atoi(argv[2]);
    int assoc = atoi(argv[3]);
    char *substituicao = argv[4];
    int flag_saida = atoi(argv[5]);
    char *arquivo_entrada = argv[6];

    // Verifica se parâmetros são potências de 2
    if (!is_potencia2(nsets) || !is_potencia2(bsize) || !is_potencia2(assoc)) {
        fprintf(stderr, "Erro: nsets, bsize e assoc devem ser potências de 2.\n");
        return 1;
    }

    // Abre o arquivo com endereços
    FILE *f = processar_arquivo(arquivo_entrada);

    inicializar_cache(nsets, assoc); // aloca cache
    inicializar_visitado();          // aloca vetor de tags visitadas

    uint32_t end;
    // Lê cada endereço de 32 bits e simula acesso
    while (fread(&end, sizeof(end), 1, f) == 1) {
        simular_acesso_cache(end, nsets, bsize, assoc, substituicao);
    }
    fclose(f); // fecha arquivo

    int flag_out = 3;
    if (!flag_saida) {
        printf("Exibir dados:\n[1] - numeros absolutos  [2] - taxas    [0] - ambos\n");
        scanf("%d", &flag_out); // lê escolha do usuário
    }

    imprimir_estatisticas(flag_saida, flag_out); // exibe resultados

    liberar_visitado(); // libera vetor de tags visitadas

    // Libera memória da cache
    for (int i = 0; i < nsets; i++) {
        free(cache[i]);
    }
    free(cache);

    return 0; // programa finalizado com sucesso
}
