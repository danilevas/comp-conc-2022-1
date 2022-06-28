#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<time.h>
#include<math.h>

/*
NOTAS SILVANA
    - sempre se ligar em quando as threads estão lendo informações globais para não haver problema de corrida
    - ver se uma thread não está alterando algo de outra thread
    - ter bem definidas as situações em que uma thread se bloqueia
    - se ligar nas condições para as threads se bloquearem e só der signal quando de fato as condições mudarem
    - pensar que o signal sinaliza a thread independente da condição que foi usada para entrar no wait

PRÓXIMOS PASSOS
    - ajeitar o erro do jogador que ataca o que já está sendo atacado por 2 e que ataca quem tá morto (colocar penalti)
    - JOGADORES ESTÃO ATACANDO DEPOIS DE MORREREM DEPOIS DO JOGO TERMINAR
    - deadlock quando um jogador está defendendo e vira um dos últimos dois no ringue
    - ajeitar a questão dos estados
    - JOGADORES FAZENDO COISAS ANTES DE LIBERAR AS THREADS CARALHO

PASSOS EVENTUAIS
    - modularizar tudo em funções
    - tirar vantagens/desvantagens de jogadores por terem um certo número (começarem antes, sempre serem atacados, etc.)
    - colocar o primeiro jogador como 1
    - balancear os valores de vida e dano, e os tempos
    - os ataques por enquanto são instantâneos, porque dá trabalho ter que lidar com a mudança de estado (um jogador inicia seu ataque contra outro quando ele está vivo, mas 
        quando chega o ataque ele já morreu). o jeito seria implementar um prejuízo para o jogador que acaba atacando outro morto ou indisponível para ataque.
    - superpoder: jogador ataca 2 outros ao mesmo tempo
    - colocar o suicídio com uma chance bem menor de acontecer (tipo um critical fail)
    - implementar acerto crítico
    - checar se a escolha de alvo está mesmo aleatória
    - ter upgrades? de dano e vida base
    - cura/poções
    - escudo de espinhos que dá dano
    - o jogo ser um jogador contra "bots" OU ser um jogo de apostas!!!!! Com descrições de cada jogador e stats pra apostar

*REGRAS DO JOGO*
    - As threads são ações: ATAQUE / DEFESA / ESQUIVA? / DESARME?
    - Vão rolando ações de jogadores aleatórios
        * Ataques contra outros jogadores (duram 0.25 segundos)
        * Defesas (duram 1 segundo)
        * Esquivas (duram 0.5 segundos)
        * Desarme (dura 0.25 segunodos)
    - Cada jogador tem vida e dano (pode ter agilidade/iniciativa, para ver quem começa primeiro (ordem de liberação das threads))
    - 2 podem atacar um defensor, que se defende dos dois
*/

#define A 2 // numero de threads de ataque POR JOGADOR
#define D 2 // numero de threads de defesa POR JOGADOR
#define D_A 2 // de quantos atacantes um defensor pode se defender ao mesmo tempo (não estou usando essa variável)
#define P 5 // total de jogadores

// TEMPOS
double tempo_defesa = 0.3;
// double tempo_ataque = 0.25;
double tempo_descanso = 0.75;
double tempo_penalti = 0.25;

int vida_minima = 25; // minimo 25
int vida_maxima = 100; // maximo 99
int dano_minimo = 10; // minimo 10
int dano_maximo = 50; // maximo 49

// variaveis do problema
int estado[P]; // vetor com os estados dos jogadores:
// 0 = INATIVO // 1 = ATACANDO // 2 = EM FORMAÇÃO DE DEFESA
int atacado_por[P]; // por quantos jogadores um jogador está sendo atacado
int vivo[P]; // 0 = morto // 1 = vivo

int threads_ativas = 0; // MAX = (A + D) * P
int espera = 0; // controle para as threads não começarem uma de cada vez
int acabado = 0;

struct Jogador {
    int id_jogador;
    int vida;
    int vida_original;
    int dano;
};

// cria a struct a ser passada para as threads: a lista de jogadores mais o jogador a que a thread pertence e o id da thread
struct Passa {
    int id_jogador;
    int id_thread;
};

// variaveis para sincronizacao
pthread_mutex_t mutex;
pthread_cond_t conds[P];

struct Jogador lista_jogs[P]; // lista global de jogadores

void acabou () {
    int mortos = 0; int o_vivo = -1;
    for(int k=0; k<P; k++) {
        if (vivo[k] == 0) {
            mortos++;
        }
        else {
            o_vivo = k;
        }
    }
    if (mortos == P - 1) {
        printf("----- ACABOU O JOGO - J%d É O GRANDE VENCEDOR ----- \n", o_vivo);
        // vivo[o_vivo] = 0;
        acabado = 1;
        exit(0);
    }
}

int IniciaAtaque (int id_jogador, int id_thread) {
    pthread_mutex_lock(&mutex);

    while (espera == 0) {
        pthread_cond_wait(&conds[id_jogador], &mutex);
    }

    // enquanto o jogador não estiver inativo ele não pode iniciar uma ação (ataque)
    while(estado[id_jogador] != 0) {
        // printf("J%d queria atacar com A%d mas está fazendo outra ação\n", id_jogador, id_thread);
        pthread_cond_wait(&conds[id_jogador], &mutex);
        // printf("J%d teve seu ataque A%d desbloqueado\n", id_jogador, id_thread);
    }

    // se o jogador tiver morrido dá um wait eterno
    while (vivo[id_jogador] == 0) {
        pthread_cond_wait(&conds[id_jogador], &mutex);
        printf("ERRO CRÍTICO - J%d MORTO ATACOU!\n", id_jogador);
    }

    // escolha do alvo
    srand(time(NULL));
    int idAlvo = 0;
    while(1) {
        idAlvo = rand() % P;
        if((idAlvo != id_jogador) && (vivo[idAlvo] == 1) && (atacado_por[idAlvo] < 2)) break;
        else if (atacado_por[idAlvo] >= 2) printf("J%d queria atacar J%d mas este já está sendo atacado por 2 jogadores\n", id_jogador, idAlvo);
        // else if (vivo[idAlvo] == 0) printf("J%d queria atacar J%d mas este está morto\n", id_jogador, idAlvo);
    }

    atacado_por[idAlvo]++;
    printf("J%d esta sendo atacado por %d jogadores - novo: J%d\n", idAlvo, atacado_por[idAlvo], id_jogador);
    
    estado[id_jogador] = 1;
    pthread_mutex_unlock(&mutex);
    return idAlvo;
}

void ExecutaAtaque (int id_jogador, int idAlvo) {
    // sleep(0.25); // tempo para realizar o ataque
    pthread_mutex_lock(&mutex);

    // se o jogador tiver morrido dá um wait eterno
    while (vivo[id_jogador] == 0) {
        printf("J%d caiu no wait do jogador morto\n", id_jogador);
        // temos que decrementar o atacado_por!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        pthread_cond_wait(&conds[id_jogador], &mutex);
        printf("ERRO CRÍTICO - J%d MORTO ATACOU!\n", id_jogador);
    }

    if (estado[idAlvo] == 2) {
    printf("DEFESA - J%d ) <=====|=o J%d\n", idAlvo, id_jogador);
    }
    if (atacado_por[idAlvo] > 2) {
        printf("ERRO - J%d atacou J%d que já estava sendo atacado por 2\n", id_jogador, idAlvo);
        // penalti
    }

    // // caso o jogador tenha morrido antes de terminar o ataque, acho q nem precisa na real
    // if (vivo[idAlvo] == 0) {
    //   printf("J%d Notou que o J%d já estava morto e desistiu!\n", id_jogador, idAlvo);
    //   // sleep(tempo_penalti); // penalizacao por atacar um morto, se confundiu
    // }

    if (vivo[idAlvo] == 0) {
    printf("ERRO - J%d atacou J%d que está morto!\n", id_jogador, idAlvo);
    // penalti
    }

    if ((estado[idAlvo] == 0 || estado[idAlvo] == 1) && vivo[idAlvo] != 0 && atacado_por[idAlvo] <= 2) {
        printf("J%d o=|=====> J%d (%dD)\n", id_jogador, idAlvo, lista_jogs[id_jogador].dano);
        lista_jogs[idAlvo].vida -= lista_jogs[id_jogador].dano;
        printf("J%d: (%d/%d)\n", idAlvo, lista_jogs[idAlvo].vida, lista_jogs[idAlvo].vida_original);

        if (lista_jogs[idAlvo].vida <= 0) {
            printf("MORTE - J%d o=|=====> J%d\n", id_jogador, idAlvo);
            vivo[idAlvo] = 0;
            printf("// VIVOS:\n");
            for (int v=0; v<P; v++) {
                if (vivo[v] == 1) {
                    printf("// J%d\n", v);
                }
            }
        }
    }
    pthread_mutex_unlock(&mutex);
}

void FimAtaque (int id_jogador, int idAlvo) {
    pthread_mutex_lock(&mutex);
    printf("J%d terminou de atacar\n", id_jogador);
    atacado_por[idAlvo]--;
    printf("J%d esta sendo atacado por %d jogadores\n", idAlvo, atacado_por[idAlvo]);
    estado[id_jogador] = 0;

    acabou();

    sleep(tempo_descanso); //tempo de descanso
    pthread_cond_signal(&conds[id_jogador]);
    pthread_mutex_unlock(&mutex);
}

void IniciaDefesa (int id_jogador, int id_thread) {
    pthread_mutex_lock(&mutex);

    while (espera == 0) {
        if (id_thread == (A + D) * P - 1) {
            espera = 1;
        }
        pthread_cond_wait(&conds[id_jogador], &mutex);
    }

    // enquanto o jogador não estiver inativo ele não pode iniciar uma ação (defesa)
    while(estado[id_jogador] != 0) {
        // printf("J%d queria defender com D%d mas está fazendo outra ação\n", id_jogador, id_thread);
        pthread_cond_wait(&conds[id_jogador], &mutex);
        // printf("J%d teve sua defesa D%d desbloqueada\n", id_jogador, id_thread);
    }

    // se o jogador tiver morrido dá um wait eterno
    while (vivo[id_jogador] == 0) {
        pthread_cond_wait(&conds[id_jogador], &mutex);
        printf("ERRO CRÍTICO - J%d MORTO DEFENDEU!\n", id_jogador);
    }

    printf("J%d quer defender\n", id_jogador);
    estado[id_jogador] = 2;
    pthread_mutex_unlock(&mutex);
}

void FimDefesa (int id_jogador) {
    pthread_mutex_lock(&mutex);
    printf("J%d terminou de defender\n", id_jogador);
    estado[id_jogador] = 0;

    // acabou();

    sleep(tempo_descanso); // tempo de descanso
    pthread_cond_signal(&conds[id_jogador]);
    pthread_mutex_unlock(&mutex);
}

// thread de ataque
void * atacante (void * arg) {
    struct Passa *passado = (struct Passa *) arg;
    int id_jogador = passado->id_jogador;
    int id_thread = passado->id_thread;

    while(1) {
        while (vivo[id_jogador] == 0) {
            printf("J%d caiu no wait\n", id_jogador);
            pthread_cond_wait(&conds[id_jogador], &mutex);
            printf("ERRO CRÍTICO - J%d MORTO ATACOU!\n", id_jogador);
        }

        int idAlvo = IniciaAtaque(id_jogador, id_thread);
        if (acabado == 1) {
            printf("J%d iria atacar mas ja acabou o jogo\n", id_jogador);
            break;
        }
        printf("J%d está atacando J%d\n", id_jogador, idAlvo);
        ExecutaAtaque(id_jogador, idAlvo);
        FimAtaque(id_jogador, idAlvo);  //tempo de descanso // SIGNAL TEM QUE SER NA THREAD MSM OU NO FINAL DO FINALATAQUE/DEFESA
    } 
    free(arg);
    pthread_exit(NULL);
}

// thread de defesa
void * defensor (void * arg) {
    struct Passa *passado = (struct Passa *) arg;
    int id_jogador = passado->id_jogador;
    int id_thread = passado->id_thread;

    while(1) {
        while (vivo[id_jogador] == 0) {
            pthread_cond_wait(&conds[id_jogador], &mutex);
            printf("ERRO CRÍTICO - J%d MORTO DEFENDEU!\n", id_jogador);
        }

        IniciaDefesa(id_jogador, id_thread);
        printf("J%d está defendendo\n", id_jogador);
        sleep(tempo_defesa); //tempo de defesa
        FimDefesa(id_jogador);
    } 
    free(arg);
    pthread_exit(NULL);
}

// funcao principal
int main(void) {
    // identificadores das threads
    pthread_t tid[(A + D) * P];
    int id[(A + D) * P];

    // inicializa as variaveis de sincronizacao
    printf("Inicializando as variaveis de sincronizacao\n");
    pthread_mutex_init(&mutex, NULL);
    for(int k=0; k<P; k++) {
        pthread_cond_init(&conds[k], NULL);
    }

    // cria os jogadores e seus status
    printf("Criando os jogadores e seus status\n");
    srand(time(NULL));
    for(int i=0; i<P; i++){
        lista_jogs[i].id_jogador = i;
        lista_jogs[i].vida = vida_minima + (rand() % (vida_maxima - vida_minima));
        lista_jogs[i].vida_original = lista_jogs[i].vida;
        lista_jogs[i].dano = dano_minimo + (rand() % (dano_maximo - dano_minimo));
        estado[i] = 0;
        vivo[i] = 1;
        atacado_por[i] = 0;
        printf("Jogador %d criado com (%d/%d) de vida e %d de dano\n", i, lista_jogs[i].vida, vida_maxima, lista_jogs[i].dano);
    }

    struct Passa passas[P][A+D];
    
    // cria as threads atacantes
    for(int j=0; j < P; j++) {
        for(int i=0; i < A; i++) {
            id[(j * A) + i] = (j * A) + i;
            passas[j][i].id_jogador = j;
            passas[j][i].id_thread = (j * A) + i;
            printf("Criando thread A%d - J%d\n", (j * A) + i, j);
            if(pthread_create(&tid[(j * A) + i], NULL, atacante, (void *) &passas[j][i])) exit(-1);
            threads_ativas++;
            //printf("Threads ativas = %d\n", threads_ativas);
            //printf("Thread atacante %d criada\n", (j * A) + i);
        } 
    }

    // cria as threads defensoras
    for(int j=0; j < P; j++) {
        for(int i=0; i < D; i++) {
            id[(A * P) + (j * D) + i] = (A * P) + (j * D) + i;
            passas[j][A+i].id_jogador = j;
            passas[j][A+i].id_thread = (A * P) + (j * D) + i;
            printf("Criando thread D%d - J%d\n", (A * P) + (j * D) + i, j);
            if(pthread_create(&tid[(A * P) + (j * D) + i], NULL, defensor, (void *) &passas[j][A+i])) exit(-1);
            threads_ativas++;
            //printf("Threads ativas = %d\n", threads_ativas);
            //printf("Thread defensora %d criada\n", (A * P) + (j * D) + i);
        }
    }
    
    sleep(0.25);
    if (espera == 1) {
        for(int jogador=0; jogador < P; jogador++) {
        printf("Threads do J%d liberadas!\n", jogador);
        pthread_cond_signal(&conds[jogador]);
        }
    }

    //  espera todas as threads terminarem
    for (int t=0; t<A+D; t++) {
        if (pthread_join(tid[t], NULL)) {
            printf("--ERRO: pthread_join()\n"); exit(-1); 
        } 
    }

    pthread_mutex_destroy(&mutex);
    pthread_exit(NULL);
    return 0;
}