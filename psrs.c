//Essa não é a implementação usada nos casos de teste e avaliada no relatório. Apenas adicionamos ela aqui pois achamos que seria interessante comparar as duas implementações.

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h> //apenas para ter números mais aleatórios
#include "timer.h"

pthread_mutex_t mutex;
pthread_cond_t cond;

//recursos compartilhados para as threads
pthread_t *tid;
int dim_por_thread;
int num_amostras; 
int contador_espera = 0;

long long int dim; 
long long int nthreads;
int *vetor_sequencial, *vetor_concorrente, *vetor_amostras, *vetor_pivot, *vetor_pivot_index, *vetor_aux;
int naoAconteceu = 1;

typedef struct{
    int id;
    int index_menor;
    int index_maior;
    int *vet;
} tArgs;



void troca(int* valor1, int* valor2)
{
    int aux = *valor1;
    *valor1 = *valor2;
    *valor2 = aux;
}

int particao(int* vet,int index_menor, int index_maior){
    int pivot = vet[index_maior];
    int i = index_menor;
    for(int j=index_menor; j<=index_maior-1; j++){ 
        if(vet[j]<pivot){ 
            troca(&vet[i], &vet[j]); 
            i++; 
        }
    }
    troca(&vet[i], &vet[index_maior]);
    return (i);
}

void quicksort(int* vet, int index_menor, int index_maior){
    
    if(index_menor < index_maior){
        
        int index_particao = particao(vet, index_menor, index_maior);

        
        quicksort(vet, index_menor, index_particao - 1);
        
        quicksort(vet, index_particao + 1, index_maior);
    }
}

void printArray(int* vet){
    for(int i = 1; i<dim; i++){
        if(vet[i]>=vet[i-1]){
            //printf(" %d", vet[i-1]);
        }
        else{
            printf("Erro: Valor fora de ordem\n");
            return;
        }
    }
    //printf(" %d\n", vet[dim-1]);
}











/*                       Sessão concorrente                      */
//parallel sorting by regular sampling
void* psrs(void *arg){
    tArgs *args = (tArgs *) arg;
    int id = args->id; //id da thread
    int *vet = args->vet; //vetor com qual a thread irá trabalhar

    //o primeiro passo do psrs é aplicar a ordenação em cada parte do array que é responsabilidade da thread como se fossem arrays isolados
    //chamamos o método de quicksort para cada thread então
    int index_menor_local = id * dim_por_thread;
    int index_maior_local = (id * dim_por_thread) + dim_por_thread - 1;
    quicksort(vet, index_menor_local, index_maior_local);

    //após essa ordenação local, podemos começar a popular o vetor de amostras
    int quant = dim_por_thread/nthreads; //quant que irá indicar "quantos elementos de cada thread" a amostra vai precisar
    //e passaremos para o vetor de amostra
    //mas step também nos ajudará a subdividir a tarefa de popular o array de amostras entre cada thread.
    for(int i = 0; i < quant; i++){
       vetor_amostras[(id * quant) + i] = vet[index_menor_local + (i * nthreads)];
    }
    
    //barreira, pois agora queremos esperar todas as threads terminarem de popular o array de amostras
    
    pthread_mutex_lock(&mutex);
    contador_espera++;
    if(contador_espera<nthreads){
        pthread_cond_wait(&cond, &mutex); 
    }else{
        pthread_cond_broadcast(&cond);
        contador_espera=0;
    }
    pthread_mutex_unlock(&mutex);

    //com o array já populado
    //uma única thread irá agora ordenar o array de amostras
    //lock para a primeira thread que chegar

    pthread_mutex_lock(&mutex);

    if(naoAconteceu){

        
        quicksort(vetor_amostras, 0, dim_por_thread-1);
        //e agora, que temos nossas amostras regulares ordenadas, precisamos escolher (número de threads - 1) pivots entre elas
        
        for(int i = 0; i <(num_amostras); i++){
            vetor_pivot[i] = vetor_amostras[(i+1) * nthreads]; //como o vetor de amostras já está ordenado, os pivos também ficarão ordenados
        }
        
        naoAconteceu = 0;
    }
    pthread_mutex_unlock(&mutex);
    //agora percorremos cada vetor local e comparamos com os pivots e marcamos onde esses pivots separam os vetores locais
    
    int j = 0;
    for(int i = index_menor_local; i<= index_maior_local; i++){
        while(j < (num_amostras) && vetor_pivot[j] < vetor_concorrente[i]){
            vetor_pivot_index[j + (id * num_amostras)] = i;
            j++;
        }
        
    }
    

    //barreira para garantir que todas as threads já tenham suas regiões delimitadas pelos pivots
    pthread_mutex_lock(&mutex);
    contador_espera++;
    if(contador_espera<nthreads){
        pthread_cond_wait(&cond, &mutex); 
    }else{
        pthread_cond_broadcast(&cond);
        contador_espera=0;
    }
    pthread_mutex_unlock(&mutex);

    //para cada thread, iremos agora cuidar de uma sessão do vetor auxiliar, formado pelas partições id +1 dos vetores de cada thread(agora tamanhos podem ser irregulares)
    //isto é, na thread 1, o vetor auxliar será composto pela partição 1 da thread 1, concatenada com a partição 1 da thread 2, concatenado com a partição 1 da thread 3... 
    
    int base = 0;

    for(int i = 0; i < id;i++){ //esse processo pode parecer um pouco chato, pois tem complexidade O(threads^2), mas para o bom uso desse algoritmo
    //número de elementos do array deve ser no mínimo threads ^2, então no pior dos casos, complexidade n
        for(int j = 0; j<nthreads;j++){
            if(i == 0){
                base += vetor_pivot_index[(j*num_amostras)] - (j * dim_por_thread); //estamos basicamente definindo a base de onde começaremos a popular o array auxiliar nesta thread
            }else{                                                                 //o começo da parte do array auxiliar por qual esta thread é responsável
                base += vetor_pivot_index[(j*num_amostras) + i] - vetor_pivot_index[(j*num_amostras) + (i - 1)];
            }
        }
    }
    
    int tam = 0; //tamanho irá definir quantos elementos desta partição teremos que colocar no nosso array, ou seja, até onde esta partição vai
    int aux_tam = 0;//aux_tam irá basicamente agir como um tamanho anterior
    int aux_pivot;
    for(int i = 0; i< nthreads; i++){
        if(id == 0){ //definindo a base da partição
            tam += vetor_pivot_index[(i*num_amostras)] - (i * dim_por_thread);
            aux_pivot = i * dim_por_thread;
        }else if(id==num_amostras){
            aux_pivot = vetor_pivot_index[(i*num_amostras) + (id - 1)];
            tam += ((i * dim_por_thread) + dim_por_thread) - aux_pivot;
            
        }else{
            aux_pivot = vetor_pivot_index[(i*num_amostras) + (id - 1)];
            tam += vetor_pivot_index[(i*num_amostras) + id] - aux_pivot;
        }
        for(int j = 0; j<tam-aux_tam;j++){ //populando o vetor auxiliar
            vetor_aux[base + aux_tam + j] = vet[aux_pivot + j];
        }
        aux_tam = tam;
    }
    
    //por fim, ordenamos nossas sessões locais do array auxiliar
    quicksort(vetor_aux, base, base + tam -1);
    
    //agora para obtermos o resultado basta apenas apontarmos o array concorrente para este array auxiliar(depois que todas as threads terminarem)
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
    tArgs *args;
    args = malloc(sizeof(tArgs)*nthreads);




    // Iniciliaza o mutex (lock de exclusao mutua) e a variavel de condicao
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);

    vetor_sequencial = malloc(sizeof(int) * dim);
    if (vetor_sequencial == NULL) {printf("ERRO--malloc\n"); return 2;}
    vetor_concorrente = malloc(sizeof(int) * dim);
    if (vetor_concorrente == NULL) {printf("ERRO--malloc\n"); return 2;}

    for (long long int i=0; i<dim; i++){
        vetor_sequencial[i] = rand()%100;
        
        vetor_concorrente[i] = vetor_sequencial[i];
    }

    
    //primeiramente, quicksort sequencial
    GET_TIME(inicio);
    quicksort(vetor_sequencial, 0, dim-1);
    GET_TIME(fim);

    printf("Vetor sequencial:\n");
    printArray(vetor_sequencial);

    tempoSeq=fim-inicio;
    printf("Tempo do sort sequencial%lf\n",tempoSeq);



    //agora, concorrente
    GET_TIME(inicio);

    //utilizando a técnica de paralallel sorting by regular sampling
    //já iremos criar as threads para dividir as tarefas, cada uma irá trabalhar em cima de subdivisões iguais do array
    //e aqui já começa a acontecer um balanceamento de cargas melhor que pelo método inocente
    dim_por_thread = dim/nthreads;

    //também já são criados vetores auxiliares que serão utilizados ao longo da execução
    vetor_amostras = malloc(sizeof(int) * dim_por_thread);
    num_amostras = nthreads-1;
    if (vetor_amostras == NULL) {printf("ERRO--malloc\n"); return 2;}
    vetor_pivot = malloc(sizeof(int) * (num_amostras));
    if (vetor_pivot == NULL) {printf("ERRO--malloc\n"); return 2;}
    vetor_pivot_index = malloc(sizeof(int) * (num_amostras) * nthreads);
    if (vetor_pivot_index == NULL) {printf("ERRO--malloc\n"); return 2;}
    vetor_aux = malloc(sizeof(int) * dim);
    if (vetor_aux == NULL) {printf("ERRO--malloc\n"); return 2;}

    tid = (pthread_t*) malloc(sizeof(pthread_t)*nthreads);
    if(tid==NULL) {puts("ERRO--malloc"); return 2;}

    for(long int i=0; i<nthreads; i++) {
        (args+i)->id = i;
        (args+i)->vet = vetor_concorrente;
        if(pthread_create((tid + i), NULL, psrs, (args + i))){
            fprintf(stderr, "ERRO--pthread_create\n");
            return 3;
        }
    }

    //aguardar o término das threads
    
    for(long int i=0; i<nthreads; i++) {
        if(pthread_join(*(tid+i),NULL)){
            fprintf(stderr, "ERRO--pthread_join\n");
            return 3;
        }
    }
    vetor_concorrente = vetor_aux;

    GET_TIME(fim);

    printf("Vetor concorrente:\n");
    printArray(vetor_concorrente);
    tempoConc = fim-inicio;
    printf("Tempo do sort concorrente%lf\n",tempoConc);

    pthread_mutex_destroy(&mutex);
    free(vetor_sequencial);
    free(vetor_concorrente);
    free(tid);

    return 0;
}
