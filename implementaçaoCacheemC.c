// SIMULADOR DE CACHE
// ARQUITETURA E ORGANIZAÇÃO DE COMPUTADORES II
// Larissa Gabriela e Vitória Santa Lucia

#include <stdio.h>   // biblioteca padrão para entrada/saída (printf, fopen, fread, etc.)
#include <stdlib.h>  // funções como malloc, free, atoi
#include <stdint.h>  // tipos inteiros com tamanho fixo (uint32_t, uint64_t)
#include <time.h>    // para inicializar o gerador randômico (srand)

//------------------------------
// Limites máximos para segurança
//------------------------------
#define MAX_ASSOC 32  // associatividade máxima da cache (vias por conjunto)
#define MAX_SETS 8192 // número máximo de conjuntos na cache

//-----------------------------------------------
// Estrutura para representar cada bloco da cache
//-----------------------------------------------
typedef struct {
    int valid;             // 1 se o bloco contém dados válidos, 0 se está vazio
    uint32_t tag;          // variavel que guarda valor de TAG/parte do endereço usada para identificar o bloco. Dira se bloco que ta mana memoria principal é mesmo da cache e se for é HIT
    int lru_counter;       // contador usado para política LRU/FIFO : funciona como linha do tempo, para saber qual bloco trocar e se atualiza a cada busca
} CacheBlock;

CacheBlock **cache; // matriz dinâmica da cache: linhas = nsets, colunas = assoc

//-----------------------------------------------
// Estrutura para rastrear os blocos já acessados
//-----------------------------------------------
typedef struct {
    uint32_t *tags;  // vetor com as chaves (índice + tag) dos blocos visitados. Guarda todas as TAGS/endereços que passaram pela cache e é importante para miss compulsorio
    int size;        // número de elementos atualmente no vetor
    int capacity;    // capacidade máxima do vetor (aumenta com realloc). Comecei com 100, mas pode aumentar
} VisitSet;

VisitSet set_visitado; // instância global para armazenar tags visitadas. Precisa ser Global

//-----------------------------------------------
// Variáveis globais para estatísticas
//-----------------------------------------------
int total_acessos = 0;      // total de acessos simulados
int hits = 0;               // número de hits
int miss_compulsorio = 0;   // misses compulsórios
int miss_total = 0;         // total de misses
int miss_conflito = 0;      // misses por conflito
int miss_capacidade = 0;    // misses por capacidade
int blocos_validos = 0;     // número de blocos válidos carregados na cache

//-----------------------------------------------
// Protótipos das funções
//-----------------------------------------------
int is_potencia2(int x); // ter certeza se é potencia de 2, pois em AOC e vida real usa Pot de 2 e inclusive para saber Offset e indice/index
void inicializar_visitado();
void liberar_visitado();
void inicializar_cache(int nsets, int assoc); //matriz
FILE* processar_arquivo(char *f);
void simular_acesso_cache(uint32_t _endereco, int nsets, int bsize, int assoc, char *sub);
void imprimir_estatisticas(int flag, int flag_out);
uint32_t inverter_big_endian(uint32_t x); // função para corrigir endianness

//-----------------------------------------------
// FUNÇÃO PARA INVERTER BIG-ENDIAN ↔ LITTLE-ENDIAN : precisa pois tava dando erro em taxa de miss,  ja que tava lendo invertido
//-----------------------------------------------
uint32_t inverter_big_endian(uint32_t x) {
    /*
     O parâmetro `x` é um número de 32 bits (4 bytes) que representaum endereço lido do arquivo binário.
    Em arquiteturas little-endian (ex.: Intel x86), os bytes são armazenados do menos significativo para o mais significativo.
    Se o arquivo foi gerado em uma máquina big-endian, precisamos trocar a ordem dos bytes para interpretar o número corretamente.
     */

    unsigned char *bytes = (unsigned char*)&x;
    /*
     `unsigned char *bytes` cria um ponteiro para os 4 bytes individuais de `x`.
      Assim podemos acessar e trocar cada byte separadamente.
     */

    unsigned char temp; // variável auxiliar para troca (swap) de bytes

    // 📌 Troca o primeiro byte (MSB) com o último (LSB)
    temp = bytes[0];
    bytes[0] = bytes[3];
    bytes[3] = temp;

    // 📌 Troca o segundo byte com o terceiro
    temp = bytes[1];
    bytes[1] = bytes[2];
    bytes[2] = temp;

    /*
     * 🟢 Por que não fazemos `bytes[1] = bytes[3]`?
     * Porque queremos inverter a ordem dos bytes simetricamente:
     * bytes[0] <-> bytes[3]  (primeiro com último)
     * bytes[1] <-> bytes[2]  (segundo com terceiro)
     * Isso garante que o número seja lido na ordem correta.
     */

    return x; // retorna o número com os bytes invertidos
}

// Verifica se um número é potência de 2
//-----------------------------------------------
int is_potencia2(int x) {
    if (x <= 0) return 0;  // números <= 0 não são potências de 2

    while (x > 1) {
        if (x % 2 != 0) return 0; // se resto != 0, não é potência de 2
        x /= 2; // divide por 2 até chegar a 1
    }

    return 1; // é potência de 2
}

// Inicializa o vetor que rastreia tags visitadas
//-----------------------------------------------
void inicializar_visitado() {
    set_visitado.tags = malloc(100 * sizeof(uint32_t)); // aloca espaço inicial para 100 tags
    set_visitado.capacity = 100; // define capacidade inicial
    set_visitado.size = 0;       // ainda não há elementos no vetor
}

//-----------------------------------------------
// Libera memória alocada para o vetor visitado
//-----------------------------------------------
void liberar_visitado() {
    free(set_visitado.tags); // libera memória do vetor
}

// Inicializa a cache como matriz dinâmica
void inicializar_cache(int nsets, int assoc) {
    cache = malloc(nsets * sizeof(CacheBlock*)); // aloca linhas/conjuntos/nsets da cache

    for (int i = 0; i < nsets; i++) {
        cache[i] = malloc(assoc * sizeof(CacheBlock)); // aloca colunas(vias) para cada linha

        for (int j = 0; j < assoc; j++) {
            cache[i][j] = (CacheBlock){0, 0, 0}; // inicializa cada bloco, pega da struct. Tag = 0 , valid = 0 e lru_counter = 0
        }
    }
    blocos_validos = 0; // nenhum bloco foi carregado ainda
}
// Abre o arquivo binário para leitura
FILE *processar_arquivo(char *arquivo_de_entrada) {
    FILE *fp = fopen(arquivo_de_entrada, "rb");

    if (!fp) {
        perror("Erro ao abrir o arquivo");
        return NULL;
    }

    return fp; // retorna ponteiro
}

// Simula o acesso de um endereço à cache
void simular_acesso_cache(uint32_t _endereco, int nsets, int bsize, int assoc, char *substituicao) {
    total_acessos++; // incrementa contador de acessos

    //------------------------------------
    // Corrige endianness do endereço lido
    //------------------------------------
    uint32_t endereco = inverter_big_endian(_endereco);
    /*
      Endereços lidos do arquivo podem estar em big-endian.
      Se o simulador está rodando em uma máquina little-endian, precisamos inverter os bytes para evitar erros no cálculo do índice/tag.
     */

    //------------------------------------
    // Calcula índice e tag
    //------------------------------------
    uint32_t endereco_sem_offset = endereco / bsize;
    /*
     * 📌 Remove os bits de offset dividindo pelo tamanho do bloco.
     * Exemplo: Se bsize = 16, os 4 bits menos significativos representam o offset.
     ou seja, sobra TAG e INDICE
     */
    uint32_t index = endereco_sem_offset % nsets;
    /*
     * 📌 Isola os bits do índice aplicando módulo nsets. = pega so os bits RESTANTES para indice
     */
    uint32_t tag = endereco_sem_offset / nsets;
    /*
     * 📌 Remove os bits do índice e offset para extrair a tag. Nao pode fazer como na teorica 32 - indice - offset , pois estamos descartando offset
//no livro de petterson, em mapeamento por conjunto,no capitulo 5, endereçamento de cache, fala que se remove o ofset dividindo por tamanho do bloco e depois
// %nsets pega indice e /nesets pega tag


    //------------------------------------
    // Verifica se o bloco já está na cache (hit)
    ?*/
    int hit = 0;

    for (int i = 0; i < assoc; i++) { // percorre todas as vias do conjunto
        if (cache[index][i].valid && cache[index][i].tag == tag) {
            hits++;     // incrementa contador de hits
            hit = 1;    // marca que foi hit

            // Atualiza contador de LRU se necessário
            if (substituicao[0] == 'L') {
                cache[index][i].lru_counter = total_acessos; // atualiza com timestamp do acesso
            }
            break; // encontrou o bloco, não precisa continuar
        }
    }

    if (hit) return; // se foi hit, não precisa carregar nada novo

    //------------------------------------
    // Miss: precisa carregar o bloco na cache
    //------------------------------------
    miss_total++; // incrementa total de misses

    //------------------------------------
    // Cria chave única combinando índice e tag
    //------------------------------------
    uint64_t chave = ((uint64_t)index << 32) | tag;
    /*
     * 📌 Essa chave é usada para rastrear se o bloco já foi acessado antes.
     * Combinar index e tag garante que blocos de conjuntos diferentes
     * não sejam confundidos.
     */

    int novo = 1; // assume que o bloco é novo

    //------------------------------------
    // Verifica se a chave já foi acessada antes, percorrendo vetor set_visitado que tem todas as tags e seus pares/chaves (indice, tag)
    //analisando se o bloco está sendo acessado pela primeira vez e, portanto, identificar se o miss é compulsório
    for (int i = 0; i < set_visitado.size; i++) {
        if (set_visitado.tags[i] == chave) {
            novo = 0; // bloco já foi acessado antes
            break;
        }
    }
    //------------------------------------
    // Se for um bloco novo, adiciona ao vetor visitado
    //------------------------------------
    //Se o bloco for novo (novo == 1), verifica se precisa aumentar capacidade
    if (novo) {
        if (set_visitado.size == set_visitado.capacity) {
            // Dobra capacidade do vetor se necessário
            set_visitado.capacity *= 2;
            set_visitado.tags = realloc(
                set_visitado.tags,
//cálculo de tamanho da memória para armazenar várias chaves.:
                set_visitado.capacity * sizeof(uint64_t) // sizeof(uint64_t) → quantos bytes cada elemento ocupa (um inteiro de 64 bits = 8 bytes).
            );
        }
        set_visitado.tags[set_visitado.size++] = chave; // Armazena a nova chave (par índice + tag) no vetor TAG e atualiza/ incrementa o tamanho
                                                            //com set_visitado.size++.
    }
//obs: O que acontece se eu usar ++size em vez de size++?”“Daria errado porque ++size incrementaria o índice antes de usar, pulando uma posição no vetor.”
    //------------------------------------
    // Verifica se o conjunto (linha da cache) está cheio
    //------------------------------------
    int set_cheio = 1;
    for (int i = 0; i < assoc; i++) { //Percorre todas as vias (i < assoc) do conjunto com índice index.
        if (!cache[index][i].valid) {//e encontrar alguma via inválida (vazia), significa que ainda tem espaço livre no conjunto.{
            set_cheio = 0; // encontrou via livre
            break;
        }
    }

    //------------------------------------
    // Classifica o tipo de miss
    //------------------------------------
    if (novo) {
        if (!set_cheio) {
            miss_compulsorio++; // primeiro acesso e há espaço livre
        } else if (blocos_validos < (long)nsets * assoc) {
            miss_conflito++;    // conjunto cheio mas cache tem espaço total(alguns outros conjuntos podem estar vazios).
        } else {
            miss_capacidade++;  // cache inteira cheia
        }
    } else {
        if (set_cheio) { //bloco valido: conta quantos blocos válidos já foram carregados na cache até agor
            if (blocos_validos < (long)nsets * assoc) { //nsets * assoc = calcula o número total de blocos na cache. e long para evitar overflow
                    //o < é: : a cache não está 100% ocupada/ ainda tem blocos disponiveis
                miss_conflito++;
            } else {
                miss_capacidade++;
            }
        } else {
            miss_conflito++; // chave revisitada e conjunto tem espaço
        }
    }

    //------------------------------------
    // Escolhe via para carregar o bloco
    //------------------------------------
    int via = -1;

    // Procura via livre
    for (int i = 0; i < assoc; i++) {
        if (!cache[index][i].valid) {
            via = i; // encontrou via livre
            break;
        }
    }

    // Se não houver via livre, aplica política de substituição
    if (via < 0) {
        if (substituicao[0] == 'R') {
            // Substituição aleatória
            via = rand() % assoc;
        } else {
            // Substituição LRU: procura via com menor contador
            via = 0;
            for (int i = 1; i < assoc; i++) {
/*Está comparando dois blocos no mesmo conjunto (set):

* cache[index][i].lru_counter: contador de acesso do bloco atual (posição i).
*cache[index][via].lru_counter: contador do bloco candidato atual à substituição.

Se o contador de i for menor, quer dizer: o bloco i foi usado há mais tempo do que o atual candidato*/

                if (cache[index][i].lru_counter < cache[index][via].lru_counter) {
                    via = i;
                }
            }
        }
    }

    //------------------------------------
    // Carrega o bloco na via selecionada, onde e cache[index][via].valid == 0 → significa bloco inválido.Então !cache[index][via].valid será TRUE (1).
    //“Essa gaveta está vazia? se for 1 ta cheia /tem algo no bloco. Se for 0, tem espaço e nao precisa substituir
    if (!cache[index][via].valid) {
        blocos_validos++; // incrementa contador de blocos válidos
    }
    cache[index][via].valid = 1;              // marca como valido e sem isso o simulador vai achar que tem bloco vazio ainda e pode sobrescrever
    cache[index][via].tag = tag;              // armazena a tag e sem atualizar a tag, a cache não consegue identificar se um endereço já está lá ou não
    cache[index][via].lru_counter = total_acessos; // atualiza total de acessos com o contador. Se não atualizar, o simulador não vai conseguir saber qual bloco é o mais antigo e pode substituir errado.

//-----------------------------------------------
// Imprime estatísticas da simulação
//-----------------------------------------------
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

    if (flag == 0) { // modo detalhado
        if (flag_out == 0 || flag_out == 1) {
            printf(
                "Total: %d\n"
                "Hits: %d\n"
                "Misses: %d\n"
                "Compulsórios: %d\n"
                "Conflitos: %d\n"
                "Capacidade: %d\n",
                total_acessos, hits, miss_total,
                miss_compulsorio, miss_conflito, miss_capacidade);
        }
        if (flag_out == 0 || flag_out == 2) {
            printf("##### TAXAS ######\n");
            printf(
                "Hit: %.4f\n"
                "Miss: %.4f\n"
                "Compulsórios: %.4f\n"
                "Conflito: %.4f\n"
                "Capacidade: %.4f\n",
                t_hit, t_miss, t_compu, t_confl, t_capac);
        }
    } else { // modo resumido
        printf(
            "%d %.4f %.4f %.4f %.4f %.4f\n",
            total_acessos, t_hit, t_miss,
            t_compu, t_confl, t_capac);
    }
}

//-----------------------------------------------
// Função principal
//-----------------------------------------------
int main(int argc, char *argv[]) {
    // Verifica se parâmetros foram fornecidos
    if (argc != 7) {
        printf("Número de argumentos incorreto. Use:\n");
        printf("./cache_simulator.exe <nsets> <bsize> <assoc> <substituição> <flag_saida> arquivo_de_entrada\n");
        return 1; // encerra com erro
    }

    srand(time(NULL)); // inicializa gerador randômico para política Random

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

    FILE *f = processar_arquivo(arquivo_entrada);
    if (!f) return 1; // erro ao abrir arquivo

    inicializar_cache(nsets, assoc); // cria cache
    inicializar_visitado();          // cria vetor visitado

    uint32_t end;
    // Lê cada endereço do arquivo e simula acesso
    while (fread(&end, sizeof(end), 1, f) == 1) {
        simular_acesso_cache(end, nsets, bsize, assoc, substituicao);
    }

    fclose(f); // fecha arquivo

    int flag_out = 3;
    if (!flag_saida) {
        printf("Exibir dados:\n[1] - números absolutos  [2] - taxas  [0] - ambos\n");
        scanf("%d", &flag_out);
    }

    imprimir_estatisticas(flag_saida, flag_out); // exibe estatísticas
    liberar_visitado();                          // libera memória do vetor visitado

    // libera memória da cache
    for (int i = 0; i < nsets; i++) {
        free(cache[i]);
    }
    free(cache);

    return 0; // sucesso
}
