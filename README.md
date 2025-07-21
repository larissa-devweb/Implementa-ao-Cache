# Simulador de Cache

Este projeto é um **Simulador de Memória Cache** desenvolvido para a disciplina **Arquitetura e Organização de Computadores II**. O objetivo é analisar o comportamento de diferentes configurações de cache (mapeamento, política de substituição, associatividade) e contabilizar hits e tipos de misses.

## Sobre o Projeto

O simulador recebe um arquivo binário contendo endereços de memória e simula o acesso à cache com os seguintes parâmetros:

* **Número de conjuntos (nsets)**
* **Tamanho do bloco (bsize)**
* **Grau de associatividade (assoc)**
* **Política de substituição (substituição)**: `LRU` (Least Recently Used) ou `RND` (Random)
* **Modo de saída (flag\_saida)**: controla o nível de detalhamento da saída.

Ele contabiliza estatísticas importantes como:

* **Total de acessos**
* **Hits**
* **Misses** (compulsórios, por conflito e por capacidade)
* **Taxas de hit e miss**

---

##  Estrutura do Projeto

* `cache_simulator.c`: Código-fonte principal do simulador.
* `Makefile`: (opcional) para compilar o projeto facilmente.
* `README.md`: documentação e instruções de uso.

---

## Compilação

Para compilar o simulador, utilize um compilador C (como `gcc`):

```bash
gcc -o cache_simulator cache_simulator.c
```

Isso gerará um executável chamado `cache_simulator`.

---

## Execução

O programa deve ser executado com os seguintes argumentos:

```bash
./cache_simulator <nsets> <bsize> <assoc> <substituição> <flag_saida> <arquivo_entrada>
```

### Parâmetros:

* `<nsets>`: número de conjuntos (deve ser potência de 2)
* `<bsize>`: tamanho do bloco em bytes (potência de 2)
* `<assoc>`: grau de associatividade (potência de 2)
* `<substituição>`: política de substituição (`LRU` ou `RND`)
* `<flag_saida>`:

  * `0`: pergunta ao usuário se quer números absolutos, taxas ou ambos.
  * `1`: imprime apenas números absolutos.
  * `2`: imprime apenas taxas.
  * `3`: imprime todos os dados direto.
* `<arquivo_entrada>`: arquivo binário com endereços de memória.

### Exemplo de execução:

```bash
./cache_simulator 64 16 4 LRU 3 enderecos.bin
```

---

## Saída do Programa

O programa exibirá:

* **Números absolutos:**

  * Total de acessos
  * Hits
  * Misses (compulsórios, por conflito, por capacidade)

* **Taxas:**

  * Taxa de hit
  * Taxa de miss
  * Proporção de cada tipo de miss

Exemplo de saída:

```
Total: 5000
Hits: 3500
Misses: 1500
Compulsórios: 800
Conflito: 400
Capacidade: 300

##### TAXAS #####
Hit: 0.7000
Miss: 0.3000
Compulsórios: 0.5333
Conflito: 0.2667
Capacidade: 0.2000
```

##Como Funciona

* **Mapeamento da cache:**

  * Calcula bits de **offset** e **índice** com `log2`.
  * Extrai **tag** para identificar blocos únicos.

* **Misses classificados como:**

  * **Compulsório**: primeira vez que o bloco é acessado.
  * **Conflito**: disputa por espaço no conjunto.
  * **Capacidade**: cache cheia e bloco foi substituído.

* **Política de Substituição:**

  * `LRU`: substitui o bloco menos recentemente usado.
  * `RND`: escolhe aleatoriamente uma via para substituir.

---
## Restrições

✅ `nsets`, `bsize` e `assoc` devem ser potências de 2.
✅ Tamanho máximo:

* `MAX_ASSOC = 32`
* `MAX_SETS = 8192`

