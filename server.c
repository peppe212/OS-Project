#include "server.h"


/** File descriptor utilizzato per la comunicazione via socket con il processo supermarket */
int supermarket_fd;

// Variabile che conta il numero di SIGPROF arrivati e non gestiti
int SIGPROF_arrived = 0;

// Mutex per le variabili globali
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

// Condition variable per informare che è arrivato un SIGPROF
static pthread_cond_t alrm_cond = PTHREAD_COND_INITIALIZER;

// Array per tenere traccia dei clienti ad ogni cassa aperta (-1 -> cassa chiusa)
int *n_clienti_per_cassa;

// Variabile booleana che indica se il supermercato è chiuso
bool supermarket_closed = false;

// Process ID del processo supermarket
pid_t mrkt_pid;

// Variabile per contare il numero di casse aperte nel supermercato
static int n_casse_aperte = 0;

// Variabile di tipo director_args_t per memorizzare i parametri del file di configurazione
static director_args_t dir_args;



int main(int argc, char* argv[]) {
    // Cancella il socket se già presente
    unlink(SOCK_NAME);
    // Alla normale terminazione del programma chiama cleanup
    atexit(cleanup);
    // Gestione dei segnali
    gestoreSegnaliDirettore();
    // Controlli sugli argomenti del programma
    if (argc != 1 && argc != 2) {
        fprintf(stderr, "Use ./%s [path_config_file]\n", argv[0]);
        return EXIT_FAILURE;
    }
    char *config_file;
    if (argc == 1)
        config_file = DEFAULT_CONF_FILE;
    else
        config_file = argv[1];
    // Se è definita THROW forka ed esegue il processo supermarket
#ifdef BUILD2
    eseguiSupermercato();
#endif
    // Si connette con un socket al processo supermarket
    int sock_fd;
    supermarket_fd = connessioneAlSupermercato(&sock_fd);
    // Comunica al processo supermarket la lunghezza del nome del file di configurazione
    scrivo_sul_socket(supermarket_fd, (int) strlen(config_file));
    // Legge il pid del processo supermarket
    mrkt_pid = leggo_dal_socket(supermarket_fd);
    // Manda il suo pid al processo supermarket
    scrivo_sul_socket(supermarket_fd, getpid());
    if (leggo_dal_socket(supermarket_fd) != RICHIESTA_NOME_FILE_CONFIGURAZIONE) error_exit()
    // Comunica al processo supermarket il file di configurazione
    syscall(writen(supermarket_fd, config_file, strlen(config_file) + 1), "writen")
    // Legge i suoi argomenti dal file di configurazione
    parsingDirettore(config_file);
    // Utilizza la select per ricevere messaggi
    fd_set rdset;
    struct timeval timeout;
    // Finché non chiude il supermercato attende le richieste e le esegue
    while (!supermarket_closed) {
        timeout.tv_sec = 0;
        timeout.tv_usec = TIMEOUT;
        FD_ZERO(&rdset);
        FD_SET(sock_fd, &rdset);
        //il timeout serve per poter uscire dal ciclo quando il supermercato chiude
        //senza il timeout rimarrei bloccato sulla select
        syscall(select(sock_fd + 1, &rdset, NULL, NULL, &timeout), "select")
        if (FD_ISSET(sock_fd, &rdset)) {
            int accepted_fd;
            syscall(accepted_fd = accept(sock_fd, NULL, 0), "accept")
            // Crea un thread worker che a seconda del messaggio svolge una determinata azione
            creaThreadWorker(accepted_fd);
        }
    }
    // Chiudo i file descriptor
    syscall(close(supermarket_fd), "close")
    syscall(close(sock_fd), "close")
    // Attesa che il processo supermarket sia terminato
#ifdef BUILD2
    syscall(waitpid(mrkt_pid, NULL, 0), "waitpid")
#else
    while (kill(mrkt_pid, 0) != -1 && errno != ESRCH)
        sleep(1);
#endif
    return EXIT_SUCCESS;
}


/*
 * Thread incaricato di gestire i segnali. Se riceve un segnale SIGHUP o SIGQUIT lo inoltra al processo supermarket,
 * mentre se riceve un segnale SIGPROF significa che il processo supermarket è pronto a ricevere un ordine di
 * apertura o chiusura di una cassa, e quindi risveglia dalla pthread_cond_wait il worker thread incaricato
 * di mandare l'ordine
 */
static void *signal_handler(void* arg) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGPROF);
    int received_signal = -1;
    // sigwait attende uno dei segnali specificati in set (SIGQUIT, SIGHUP o SIGPROF) e setta received_signal
    // con il segnale ricevuto
    while (!supermarket_closed) {
        ec_nzero(sigwait(&set, &received_signal))
        mutex_LOCK(&mtx)
        // Sceglie che azione intraprendere in base al segnale ricevuto
        switch (received_signal) {
            case SIGHUP:
                syscall(kill(mrkt_pid, SIGHUP), "kill")
                mutex_UNLOCK(&mtx)
                break;

            case SIGQUIT:
                syscall(kill(mrkt_pid, SIGQUIT), "kill")
                mutex_UNLOCK(&mtx)
                break;

            case SIGPROF:
                SIGPROF_arrived++;
                ec_nzero(pthread_cond_signal(&alrm_cond))
                mutex_UNLOCK(&mtx)
                break;

            default: error_exit()
        }
    }
    return NULL;
}


/* Gestione dei segnali */
void gestoreSegnaliDirettore() {
    // Maschera i segnali
    sigset_t sigmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGHUP);
    sigaddset(&sigmask, SIGQUIT);
    sigaddset(&sigmask, SIGPROF);
    syscall(pthread_sigmask(SIG_BLOCK, &sigmask, NULL), "pthread_sigmask")
    // Ignora SIGPIPE
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    syscall(sigaction(SIGPIPE, &sa, NULL), "sigaction")
    // Crea un thread per la gestione dei segnali in modalità detached
    pthread_t signal_tid;
    pthread_attr_t attr;
    ec_nzero(pthread_attr_init(&attr))
    ec_nzero(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
    ec_nzero(pthread_create(&signal_tid, &attr, signal_handler, NULL))
}


/* Forka ed esegue il processo supermarket */
void eseguiSupermercato() {
    pid_t pid;
    syscall(pid = fork(), "fork")
    if (pid == 0) {
        execl(MRKT_PATH, "./client", NULL);
        error("execl")
    }
}


/* Crea un socket e si connette al processo supermarket */
int connessioneAlSupermercato(int *sock_fd) {
    int s_mrkt_fd;
    syscall(*sock_fd = socket(AF_UNIX, SOCK_STREAM, 0), "director socket")
    struct sockaddr_un sa;
    strncpy(sa.sun_path, SOCK_NAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;
    syscall(bind(*sock_fd, (struct sockaddr *) &sa, sizeof(sa)), "bind")
    syscall(listen(*sock_fd, SOMAXCONN), "listen")
    syscall(s_mrkt_fd = accept(*sock_fd, NULL, 0), "accept")
    return s_mrkt_fd;
}


/* Legge i parametri dal file di configurazione */
void parsingDirettore(char* config_fname) {
    FILE *config_fp = NULL;
    if ((config_fp = fopen(config_fname, "r")) == NULL) error("fopen")
    char buffer[BUFSIZ], *rtn, *delimiter;
    int i = 0;
    while (true) {
        i++;
        rtn = fgets(buffer, BUFSIZ, config_fp);
        if (rtn == NULL && !feof(config_fp)) error("fgets")
        else if (rtn == NULL) break;
        if (i != 1 && i != 7 && i != 11 && i != 12) continue;
        delimiter = strchr(buffer, ':');
        delimiter = delimiter + 2;
        switch (i) {
            case 1:
                dir_args.max_casse_aperte = (int) strtol(delimiter, NULL, 10);
                malloc_(n_clienti_per_cassa, dir_args.max_casse_aperte, int)
                for (int j = 0; j < dir_args.max_casse_aperte; ++j)
                    n_clienti_per_cassa[j] = -1;
                break;

            case 7:
                n_casse_aperte = (int) strtol(delimiter, NULL, 10);
                for (int j = 0; j < n_casse_aperte; ++j)
                    n_clienti_per_cassa[j] = 0;
                break;

            case 11:
                dir_args.s_chiusura_cassa = (int) strtol(delimiter, NULL, 10);
                break;

            case 12:
                dir_args.s_apertura_cassa = (int) strtol(delimiter, NULL, 10);
                break;

            default: error_exit()
        }
    }
    ec_nzero(fclose(config_fp))
}

/*
 * Aux function di seleziona_compito(): decide se aprire o chiudere una cassa, o se non fare niente. Se decide di
 * aprire o chiudere una cassa manda un segnale SIGPROF al supermercato, attende un SIGPROF di risposta e successivamente
 * scrive sul socket la decisione presa
 */
static void decision() {
    bool chiusura = false, apertura = false;
    int contatore_casse_max_un_cliente = 0, i = 0, ind_cassa = -1;
    // Decide se aprire o chiudere una cassa in base alle soglie descritte nel file di configurazione
    while (i < dir_args.max_casse_aperte && !chiusura && !apertura) {
        if (n_clienti_per_cassa[i] >= dir_args.s_apertura_cassa) {
            apertura = true;
        } else if (n_clienti_per_cassa[i] == 0 || n_clienti_per_cassa[i] == 1) {
            contatore_casse_max_un_cliente++;
            if (contatore_casse_max_un_cliente >= dir_args.s_chiusura_cassa) {
                ind_cassa = i;
                chiusura = true;
            }
        }
        i++;
    }
    // Manda il messaggio al processo supermarket con la decisione presa
    int dec;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGPROF);
    if (apertura) {
        // Scelta della cassa da aprire
        i = 0;
        while (i < dir_args.max_casse_aperte && ind_cassa == -1) {
            if (n_clienti_per_cassa[i] == -1)
                ind_cassa = i;
            else
                i++;
        }
        // Se l'ha trovata (vero se non sono già aperte il numero massimo di casse) allora manda l'ordine di apertura
        if (ind_cassa != -1) {
            // Manda SIGPROF a supermarket per informarlo che deve mandare un ordine
            syscall(kill(mrkt_pid, SIGPROF), "kill")
            // Attende un SIGPROF da supermarket per inviare l'ordine
            while (!SIGPROF_arrived)
                ec_nzero(pthread_cond_wait(&alrm_cond, &mtx))
            SIGPROF_arrived--;
            dec = ORDINE_APERTURA_CASSA;
            scrivo_sul_socket(supermarket_fd, ORDINE_APERTURA_CASSA);
        } else return;
    } else if (chiusura && n_casse_aperte > 1) {    // Qui l'indice della cassa da chiudere l'ho già scelto prima
        // Manda SIGPROF a supermarket per informarlo che deve mandare un ordine
        syscall(kill(mrkt_pid, SIGPROF), "kill")
        // Attende un SIGPROF da supermarket per inviare l'ordine
        while (!SIGPROF_arrived)
            ec_nzero(pthread_cond_wait(&alrm_cond, &mtx))
        SIGPROF_arrived--;
        dec = ORDINE_CHIUSURA_CASSA;
        scrivo_sul_socket(supermarket_fd, ORDINE_CHIUSURA_CASSA);
    } else return;
    // Il processo supermarket chiede su quale cassa deve eseguire l'ordine
    if (leggo_dal_socket(supermarket_fd) != RICHIESTA_NUMERO_CASSA) error_exit()
    // Invia il numero della cassa al processo supermarket
    scrivo_sul_socket(supermarket_fd, ind_cassa);
    // Aggiorna il numero di casse aperte e il numero di clienti per ogni cassa
    if (dec == ORDINE_APERTURA_CASSA && leggo_dal_socket(supermarket_fd) == APERTURA_CASSA_COMPLETATA) {
        n_casse_aperte++;
        n_clienti_per_cassa[ind_cassa] = 0;
    } else if (dec == ORDINE_CHIUSURA_CASSA && leggo_dal_socket(supermarket_fd) == CHIUSURA_CASSA_COMPLETATA) {
        n_casse_aperte--;
        n_clienti_per_cassa[ind_cassa] = -1;
    }
}


/* Thread creato da creaThreadWorker: riceve la richiesta dal supermercato e la esegue */
static void *seleziona_compito(void* arg) {
    int *fd = arg;
    mutex_LOCK(&mtx)
    int received_msg = leggo_dal_socket(*fd);
    switch (received_msg) {
        // Aggiornamento sul numero di clienti in coda ad una cassa
        case RICHIESTA_NUMERO_CLIENTI:
            scrivo_sul_socket(*fd, RICHIESTA_NUMERO_CASSA);
            int ind = leggo_dal_socket(*fd);
            scrivo_sul_socket(*fd, RICHIESTA_TOTALE_CLIENTI_IN_CODA);
            n_clienti_per_cassa[ind] = leggo_dal_socket(*fd);
            scrivo_sul_socket(*fd, AGGIORNAMENTO_COMPLETATO);
            decision();
            mutex_UNLOCK(&mtx)
            break;

            // Richiesta di uscita senza acquisti da parte di un cliente
        case RICHIESTA_USCITA_SENZA_ACQUISTI:
            scrivo_sul_socket(*fd, RICHIESTA_NUMERO_PRODOTTI);
            int n_prod = leggo_dal_socket(*fd);
            if (n_prod == 0)
                scrivo_sul_socket(*fd, PERMESSO_CONCESSO);
            else
                scrivo_sul_socket(*fd, PERMESSO_NEGATO);
            mutex_UNLOCK(&mtx)
            break;

            // Notifica del supermercato sul fatto che ha chiuso
        case CHIUSURA_SUPERMERCATO:
            supermarket_closed = true;
            syscall(close(*fd), "close")
            mutex_UNLOCK(&mtx)
            break;

        default: error_exit()
    }
    // Lavoro completato
    return NULL;
}


/* Crea un thread worker in modalità detached */
void creaThreadWorker(int fd) {
    pthread_t tid;
    pthread_attr_t attr;
    ec_nzero(pthread_attr_init(&attr))
    ec_nzero(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
    ec_nzero(pthread_create(&tid, &attr, seleziona_compito, &fd))
}


/* Libera la memoria allocata dinamicamente e cancella il socket */
void cleanup() {
    unlink(SOCK_NAME);
    free(n_clienti_per_cassa);
}