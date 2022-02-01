#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h> //apenas para ter números mais aleatórios
#include "timer.h"

pthread_mutex_t mutex;

//recursos compartilhados para as threads
int threads_criadas = 0;
pthread_t *tid; //identificadores das threads no sistema

long long int dim; //dimensao do vetor de entrada
long long int nthreads;
int *vetor_sequencial, *vetor_concorrente, *vetor_temp;

typedef struct{
    int index_menor;
    int index_maior;
    int *vet;
} tArgs;







//parte do código mais sequencial(embora seja usado nas threads a lógica em si é sequencial)
//função auxiliar para trocar dois elementos do array de lugar
void troca(int* valor1, int* valor2)
{
    int aux = *valor1;
    *valor1 = *valor2;
    *valor2 = aux;
}

int particao(int* vet,int index_menor, int index_maior){
    //escolhendo o último elemento como o pivot(poderia ser qualquer outro)
    int pivot = vet[index_maior];

    //i irá indicar a posição onde o pivot estará ordenado, com elementos menores a sua esquerda e maiores a sua direita
    //por isso, sempre que um elemento maior que o pivot aparecer, i será incrementada.
    int i = index_menor; //iniciamos i com o index do primeiro elemento, pois se nenhum elemento menor que o pivot aparecer, o pivot irá virar o primeiro elemento em ordenação.

    for(int j=index_menor; j<=index_maior-1; j++){ //percorremos o array procurando valores menores que o pivot
        if(vet[j]<pivot){ //se um valor for menor que o pivot, ele deve ficar a sua esquerda
            troca(&vet[i], &vet[j]); //então trocamos as posições
            i++; //e incrementamos i
        }
    }

    //colocamos o pivot em seu devido lugar
    troca(&vet[i], &vet[index_maior]);
    //e retornamos o index do pivot
    return (i);
}

//quicksort de forma recursiva
//a ideia do quicksort é escolher um pivot
//após isso, percorrer o array e garantir que este pivot fique na sua posição certa
//então repartir o array em 2 partições, uma abaixo desse pivot e outra acima desse pivot
//e finalmente, repetir o processo acima com as repartições resultantes até o array estar ordenado

void quicksort(int* vet, int index_menor, int index_maior){
    
    //falhar ao entrar neste if quer dizer que a partição já esta ordenada, pois os possíveis casos são:
    //menor > maior, então o pivot escolhido já é o menor ou maior elemento desta partição e já foi colocado em seu devido lugar pela função "particao"
    //menor = maior, o pivot escolhido era o segundo menor ou segundo maior elemento desta partição e já foi colocado em seu devido lugar pelo função "particao"
    //o que nos deixa com um partição vazia ou com um elemento só, ou seja, já ordenada
    if(index_menor < index_maior){
        //enfim, se a partição ainda não está ordenada
        //chamamos o método partição
        int index_particao = particao(vet, index_menor, index_maior);

        //chamada recursiva para a partição a esquerda do pivot
        quicksort(vet, index_menor, index_particao - 1);
        //chamada recursiva para a partição a direita do pivot
        quicksort(vet, index_particao + 1, index_maior);
    }
}

//essa função vai apenas passar por cada elemento do vetor e compará-lo com o elemento anterior.
//se o elemento for maior que o anterior, quer dizer que está ordenado e será printado na tela.
//se não, um aviso de que um valor fora de ordem foi achado será printado e a função termina.
void checaArray(int* vet){
    for(int i = 1; i<dim; i++){
        if(vet[i]<vet[i-1]){
            printf("Erro: Valor fora de ordem\n");
            return;
        }
    }
    printf("Todos os valores estão em ordem.\n");
    
}











/*                       Sessão concorrente                      */


//função que irá seguir a mesma lógica do quicksort
//escolher um pivot e dividir o array em 2
//a diferença é que enquanto puderem ser criadas mais threads
//a thread atual ficará responsável por uma das subdivisões, e outra thread será criada para outra subdivisão
//se mais threads não puderem ser criadas, cada thread irá executar a função de quicksort normal.
void* quicksort_concorrente(void *arg){
    tArgs *args = (tArgs *) arg;
    int index_menor= args->index_menor;
    int index_maior= args->index_maior;
    int *vet = args->vet;
    
    if(index_menor < index_maior){
        int index_particao = particao(vet, index_menor, index_maior);
        //para não passar do limite de threads
        pthread_mutex_lock(&mutex);
        if(threads_criadas < nthreads){
            tArgs args = {index_menor, index_particao-1, vet};
            pthread_create(&tid[threads_criadas], NULL, quicksort_concorrente, &args);
            threads_criadas++;
            pthread_mutex_unlock(&mutex);
            tArgs args2 = {index_particao + 1, index_maior, vet};
            quicksort_concorrente(&args2);
        
        
        }else{//já criamos o número de threads definidos, cada thread pode executar o quicksort normalmente
            pthread_mutex_unlock(&mutex);
            quicksort(vet, index_menor, index_particao - 1);
            quicksort(vet, index_particao + 1, index_maior);
        }

        
    }
    pthread_exit(NULL);
}









/*                          Main                           */

int main(int argc, char* argv[]) {

    if(argc<3) {
        printf("Digite: %s <numero de elementos do vetor> <numero maximo de threads>\n", argv[0]);
        return 1;
    }
    dim = atoll(argv[1]);
    nthreads = atoll(argv[2]);
    


    tid = (pthread_t*) malloc(sizeof(pthread_t)*nthreads);
    if(tid==NULL) {puts("ERRO--malloc"); return 2;}
    
    double inicio, fim, tempoSeq, tempoConc; //tomada de tempo
    tArgs args;




    // Iniciliaza o mutex (lock de exclusao mutua) e a variavel de condicao
    pthread_mutex_init(&mutex, NULL);

    vetor_sequencial = malloc(sizeof(int) * dim);
    if (vetor_sequencial == NULL) {printf("ERRO--malloc\n"); return 2;}
    vetor_concorrente = malloc(sizeof(int) * dim);
    if (vetor_concorrente == NULL) {printf("ERRO--malloc\n"); return 2;}
    
    vetor_temp = malloc(sizeof(int) * dim);
    if (vetor_temp == NULL) {printf("ERRO--malloc\n"); return 2;}

    for (long long int i=0; i<dim; i++){
        vetor_sequencial[i] = rand()%100;
        vetor_concorrente[i] = vetor_sequencial[i];
    }
    
    //primeiramente, quicksort sequencial
    GET_TIME(inicio);
    quicksort(vetor_sequencial, 0, dim-1);
    GET_TIME(fim);

    printf("Vetor sequencial:\n");
    checaArray(vetor_sequencial);

    tempoSeq=fim-inicio;

    GET_TIME(inicio);

    tid = (pthread_t*) malloc(sizeof(pthread_t)*nthreads);
    if(tid==NULL) {puts("ERRO--malloc"); return 2;}

    args.vet=vetor_concorrente;
    args.index_menor=0;
    args.index_maior=dim-1;


    //poderíamos chamar a própria função quicksort_concorrente aqui e deixar a thread principal participar junto com as outras threads, 
    //mas para mais organização, preferimos deixar o trabalho apenas para as threads criadas.
    
    //incrementando a variável antes, só para ter garantia de que o código ainda é sequencial
    threads_criadas ++;
    pthread_create(&tid[threads_criadas-1], NULL, quicksort_concorrente, &args); 

    //aguardar o termino das threads
    for(long int i=0; i<nthreads; i++) {
        if(pthread_join(*(tid+i),NULL)){
            fprintf(stderr, "ERRO--pthread_create\n");
            return 3;
        }
    }

    GET_TIME(fim);

    printf("Vetor concorrente:\n");
    checaArray(vetor_concorrente);
    tempoConc = fim-inicio;

    printf("Delta sequencial: %lf\n", tempoSeq);
    printf("Delta concorrente: %lf\n", tempoConc);
    printf("Aceleração(Delta Sequencial / Delta Concorrente):%lf\n", (tempoSeq/tempoConc));

    pthread_mutex_destroy(&mutex);
    free(vetor_sequencial);
    free(vetor_concorrente);
    free(vetor_temp);
    free(tid);

    return 0;
}
