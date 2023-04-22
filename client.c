#include "client.h"


// File descriptor per la comunicazione via socket utilizzato da threadOperaio()
int dir_mrkt_fd;

// Process ID del processo director
pid_t dir_pid;

// Variabile booleana che indica se l'ingresso del supermercato è aperto
bool accesso_consentito = true;

// Variabile booleana che indica se è in corso una evacuazione di emergenza
bool chiusura_immediata = false;

// Mutex globale utilizzato per la mutua esclusione sulle variabili globali
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

// Condition variable che informa dell'uscita di un cliente dal supermercato
pthread_cond_t exit_cond = PTHREAD_COND_INITIALIZER;

// Variabile contatore per sapere quanti threads sono attivi
int threads_counter = 0;

// Variabile contatore per sapere quanti clienti sono all'interno del supermercato
int n_clienti_dentro = 0;

// Variabile contatore per sapere quante casse sono aperte
int n_casse_aperte = 0;

// Variabile che memorizza il numero di clienti totali entrati nel supermercato
int tot_clienti = 0;

// Variabile che memorizza il numero totale di articoli acquistati
int tot_prodotti = 0;

// Thread ID del thread incaricato di gestire gli ingressi nel supermercato
pthread_t entry_tid;

// Array di casse, la cui dimensione è specificata nel file di configurazione (il max di casse aperte)
cassa_t *casse;

// Coda contenente tutti i dati dei clienti usciti dal supermercato
queue_t *coda_clienti_usciti;

// Variabile contenente tutti i parametri letti nel file di configurazione
supermarket_args_t s_market_args;



int main(int argc, char* argv[]) {
    // Alla normale terminazione del programma chiama cleanup
    atexit(cleanup);
    // Gestione dei segnali
    gestoreSegnaliMarket();
    // Controlli sugli argomenti del programma
    if (argc != 1) {
        fprintf(stderr, "Use ./%s\n", argv[0]);
        return EXIT_FAILURE;
    }
    // Si connette con un socket al processo director
    dir_mrkt_fd = connessioneAlDirettore();
    // Riceve la lunghezza del nome del file di configurazione
    int config_len = leggo_dal_socket(dir_mrkt_fd);
    // Manda al processo director il suo pid
    pid_t pid = getpid();
    scrivo_sul_socket(dir_mrkt_fd, pid);
    // Riceve dal processo director il pid di director
    dir_pid = leggo_dal_socket(dir_mrkt_fd);
    scrivo_sul_socket(dir_mrkt_fd, RICHIESTA_NOME_FILE_CONFIGURAZIONE);
    // Riceve dal processo director il nome del file di configurazione
    char config_fname[BUFSIZ];
    syscall(readn(dir_mrkt_fd, config_fname, config_len + 1), "readn")
    // Legge i suoi argomenti dal file di configurazione
    parsingMarket(config_fname);

    /* INIZIA LA GIORNATA DI LAVORO */
    // Apertura del supermercato alla clientela
    aperturaSupermercato();
    // Finché non arriva un segnale di chiusura continua la giornata di lavoro
    int received_signal;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGPROF);
    bool time_to_close = false;
    while (!time_to_close) {
        ec_nzero(sigwait(&set, &received_signal))
        // Arrivato un segnale
        switch (received_signal) {
            case SIGQUIT:
                chiusuraImmediataSupermercato();
                time_to_close = true;
                break;

            case SIGHUP:
                chiusuraSupermercato();
                time_to_close = true;
                break;

            case SIGPROF: {
                // Se arriva un segnale di SIGPROF significa che il direttore vuole aprire o chiudere una cassa,
                // e quindi viene creato un thread incaricato di portare a termine il compito
                pthread_attr_t attr;
                ec_nzero(pthread_attr_init(&attr))
                ec_nzero(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
                pthread_t tid;
                ec_nzero(pthread_create(&tid, &attr, threadOperaio, NULL))
                break;
            }

            default: error_exit()
        }
    }
    // Crea il file log
    scriviLogfile();
    syscall(close(dir_mrkt_fd), "close")
    return EXIT_SUCCESS;
}


/* Gestione dei segnali */
void gestoreSegnaliMarket() {
    // Maschero i segnali SIGHUP, SIGQUIT e SIGPROF
    sigset_t sigmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGHUP);
    sigaddset(&sigmask, SIGQUIT);
    sigaddset(&sigmask, SIGPROF);
    syscall(pthread_sigmask(SIG_BLOCK, &sigmask, NULL), "pthread_sigmask")
    // Ignoro SIGPIPE
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    syscall(sigaction(SIGPIPE, &sa, NULL), "sigaction")
}


/* Connessione al direttore via socket */
int connessioneAlDirettore() {
    int dm_fd;
    syscall(dm_fd = socket(AF_UNIX, SOCK_STREAM, 0), "supermarket socket")
    struct sockaddr_un sa;
    strncpy(sa.sun_path, SOCK_NAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;
    while (connect(dm_fd, (struct sockaddr *) &sa, sizeof(sa)) == -1) {
        if (errno == ENOENT)    // Il socket non esiste ancora
            sleep(1);
        else error("connect")
    }
    return dm_fd;
}


/* Lettura dei parametri descritti nel file di configurazione */
void parsingMarket(char* config_fname) {
    FILE *config_fp = NULL;
    if ((config_fp = fopen(config_fname, "r")) == NULL) error("fopen")
    char buffer[BUFSIZ], *rtn, *delimiter;
    int i = 0;
    while (true) {
        i++;
        rtn = fgets(buffer, BUFSIZ, config_fp);
        if (rtn == NULL && !feof(config_fp)) error("fgets")
        else if (rtn == NULL) break;
        if (i == 11 || i == 12) continue;
        delimiter = strchr(buffer, ':');
        delimiter = delimiter + 2;
        switch (i) {
            case 1:
                s_market_args.max_casse_aperte = (int) strtol(delimiter, NULL, 10);
                break;

            case 2:
                s_market_args.max_clienti = (int) strtol(delimiter, NULL, 10);
                break;

            case 3:
                s_market_args.soglia_clienti = (int) strtol(delimiter, NULL, 10);
                break;

            case 4:
                s_market_args.tempo_decisione_cliente = (int) strtol(delimiter, NULL, 10);
                break;

            case 5:
                s_market_args.tempo_acquisti_cliente = (int) strtol(delimiter, NULL, 10);
                break;

            case 6:
                s_market_args.max_prodotti_cliente = (int) strtol(delimiter, NULL, 10);
                break;

            case 7:
                s_market_args.n_casse_apertura = (int) strtol(delimiter, NULL, 10);
                break;

            case 8: malloc_(s_market_args.log_fname, BUFSIZ, char)
                strncpy(s_market_args.log_fname, delimiter, BUFSIZ);
                s_market_args.log_fname[strlen(s_market_args.log_fname) - 1] = '\0';
                break;

            case 9:
                s_market_args.tempo_cassiere_singolo_prodotto = (int) strtol(delimiter, NULL, 10);
                break;

            case 10:
                s_market_args.intrv_resoconto_director = (int) strtol(delimiter, NULL, 10);
                break;

            default: error_exit()
        }
    }
    ec_nzero(fclose(config_fp))
}


long tempoInMillisecondi() {
    struct timeval time;
    syscall(gettimeofday(&time, NULL), "gettimeofday")
    return (time.tv_sec * 1000) + (time.tv_usec / 1000);
}


/*
 * Thread creato da aperturaSupermercato() e incaricato di far entrare i clienti ogni volta che ne esce la soglia descritta
 * nel file di configurazione
 */
static void* entry_handler(void* arg) {
    // Finché l'ingresso è aperto ai clienti e non è in corso una evacuazione
    while (accesso_consentito && !chiusura_immediata) {
        mutex_LOCK(&mtx)
        // Finché non è uscita la soglia di clienti descritta nel file di configurazione aspetta
        while (accesso_consentito && !chiusura_immediata &&
               n_clienti_dentro > s_market_args.max_clienti - s_market_args.soglia_clienti)
            ec_nzero(pthread_cond_wait(&exit_cond, &mtx))
        // Se nel frattempo è stato chiuso l'ingresso o è in corso un'evacuazione esce
        if (!accesso_consentito || chiusura_immediata) {
            mutex_UNLOCK(&mtx)
            break;
        }
        // Fa entrare clienti finché non viene raggiunto il numero massimo
        while (n_clienti_dentro != s_market_args.max_clienti) {
            cliente_t *client;
            calloc_(client, 1, cliente_t)
            pthread_attr_t attr;
            ec_nzero(pthread_attr_init(&attr))
            ec_nzero(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
            ec_nzero(pthread_create(&client->tid, &attr, customer, client))
            n_clienti_dentro++;
        }
        mutex_UNLOCK(&mtx)
    }
    return NULL;
}


/*
 * Apre il supermercato inizializzando tutti i parametri necessari e creando i threads cassieri, i threads clienti e
 * un thread incaricato della gestione degli ingressi
 */
void aperturaSupermercato() {
    // Inizializzazione
    calloc_(casse, s_market_args.max_casse_aperte, cassa_t)
    coda_clienti_usciti = init_queue();
    if (coda_clienti_usciti == NULL) error("init_queue")
    for (int i = 0; i < s_market_args.max_casse_aperte; ++i) {
        casse[i].num_cassa = i;
        casse[i].n_chiusure = 0;
        casse[i].n_clienti_serviti = 0;
        casse[i].clienti_in_coda = 0;
        casse[i].n_prod_elaborati = 0;
        casse[i].aperta = false;
        casse[i].ordinata_chiusura = false;
        casse[i].tempo_per_ogni_cliente = init_queue();
        casse[i].tempi_aperture = init_queue();
        if (casse[i].tempo_per_ogni_cliente == NULL
            || casse[i].tempi_aperture == NULL) error("init_queue")
        ec_nzero(pthread_cond_init(&casse[i].cond, NULL))
    }
    mutex_LOCK(&mtx)
    pthread_attr_t attr;
    ec_nzero(pthread_attr_init(&attr))
    ec_nzero(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
    // Apre il numero di casse descritto nel file di configurazione creando dei threads cassieri in modalità detached
    for (int i = 0; i < s_market_args.n_casse_apertura; ++i) {
        casse[i].coda = init_queue();
        if (casse[i].coda == NULL) error("init_queue")
        ec_nzero(pthread_create(&casse[i].tid, &attr, cassiere, &casse[i].num_cassa))
        n_casse_aperte++;
        casse[i].aperta = true;
    }
    // Fa entrare il numero massimo di clienti creando dei threads in modalità detached
    for (int i = 0; i < s_market_args.max_clienti; ++i) {
        cliente_t *client;
        calloc_(client, 1, cliente_t)
        ec_nzero(pthread_create(&client->tid, &attr, customer, client))
        n_clienti_dentro++;
    }
    ec_nzero(pthread_attr_destroy(&attr))
    // Crea un ultimo thread incaricato di far entrare nuovi clienti una volta che ne è uscita la soglia stabilita
    ec_nzero(pthread_create(&entry_tid, NULL, entry_handler, NULL))
    mutex_UNLOCK(&mtx)
}



/*
 * Thread incaricato di ricevere un ordine di chiusura o apertura di una cassa dal direttore. Se è un ordine di apertura
 * allora crea un thread cassiere e la apre. Se invece è un ordine di chiusura allora informa la cassa che deve chiudere
 * settando a true il campo ordinata_chiusura della cassa.
 */
void* threadOperaio(void* arg) {
    // Nel momento in cui acquisisce il mutex sicuramente nessun altro thread sta scambiando messaggi
    // con il processo director
    mutex_LOCK(&mtx)
    threads_counter++;
    // Informa il direttore che è pronto a ricevere l'ordine restituendo un segnale SIGPROF
    syscall(kill(dir_pid, SIGPROF), "kill")
    // Riceve l'ordine
    int order = leggo_dal_socket(dir_mrkt_fd);
    // Richiede il numero della cassa
    scrivo_sul_socket(dir_mrkt_fd, RICHIESTA_NUMERO_CASSA);
    // Lo riceve
    int num_cassa = leggo_dal_socket(dir_mrkt_fd);
    // Decisione dell'azione in base all'ordine ricevuto
    bool ok = false;
    switch (order) {
        case ORDINE_APERTURA_CASSA:
            // Apre la cassa se non sono già aperte tutte le casse, o se il supermercato non è in chiusura
            if (n_casse_aperte == s_market_args.max_casse_aperte || casse[num_cassa].aperta ||
                !accesso_consentito || chiusura_immediata) {
                scrivo_sul_socket(dir_mrkt_fd, APERTURA_CASSA_FALLITA);
                ok = false;
            } else {
                apertura_cassa(num_cassa);
                scrivo_sul_socket(dir_mrkt_fd, APERTURA_CASSA_COMPLETATA);
                ok = true;
            }
            break;

        case ORDINE_CHIUSURA_CASSA:
            // Manda l'ordine alla cassa se non è l'unica aperta
            if (n_casse_aperte == 1 || !casse[num_cassa].aperta || casse[num_cassa].ordinata_chiusura) {
                scrivo_sul_socket(dir_mrkt_fd, CHIUSURA_CASSA_FALLITA);
                ok = false;
            } else {
                // Controlla se è stata già ordinata la chiusura a tutte le casse tranne una
                int counter = 0;
                for (int i = 0; i < s_market_args.max_casse_aperte; ++i) {
                    if (casse[i].aperta && casse[i].ordinata_chiusura) counter++;
                }
                // Nel caso non manda l'ordine di chiusura
                if (counter == n_casse_aperte - 1) {
                    scrivo_sul_socket(dir_mrkt_fd, CHIUSURA_CASSA_FALLITA);
                    ok = false;
                } else {
                    casse[num_cassa].ordinata_chiusura = true;
                    scrivo_sul_socket(dir_mrkt_fd, CHIUSURA_CASSA_COMPLETATA);
                    ok = true;
                }
            }
            break;

        default: error_exit()
    }
    threads_counter--;
    // Se è stato mandato l'ordine di chiusura ad una cassa in attesa di clienti allora viene risvegliata
    // dalla pthread_cond_wait
    if (order == ORDINE_CHIUSURA_CASSA && ok)
        ec_nzero(pthread_cond_signal(&casse[num_cassa].cond))
    mutex_UNLOCK(&mtx)
    return NULL;
}


/* Fa chiudere il supermercato, senza attendere che i clienti all'interno terminino gli acquisti */
void chiusuraImmediataSupermercato() {
    mutex_LOCK(&mtx)
    accesso_consentito = false;
    chiusura_immediata = true;
    // Manda un segnale al thread incaricato della gestione delle entrate per svegliarlo dalla wait
    ec_nzero(pthread_cond_signal(&exit_cond))
    mutex_UNLOCK(&mtx)
    // Attende che termini il thread incaricato della gestione delle entrate
    ec_nzero(pthread_join(entry_tid, NULL))
    // Attende che siano usciti tutti i clienti
    mutex_LOCK(&mtx)
    while (n_clienti_dentro != 0)
        ec_nzero(pthread_cond_wait(&exit_cond, &mtx))
    // Manda un ordine di chiusura a tutte le casse
    for (int i = 0; i < s_market_args.max_casse_aperte; ++i) {
        if (!casse[i].ordinata_chiusura)
            casse[i].ordinata_chiusura = true;
    }
    // Risveglia tutte le casse che attendevano sulla pthread_cond_wait
    while (threads_counter != 0) {
        for (int i = 0; i < s_market_args.max_casse_aperte; ++i) {
            ec_nzero(pthread_cond_signal(&casse[i].cond))
            mutex_UNLOCK(&mtx)
            mutex_LOCK(&mtx)
        }
    }
    // Notifica al direttore che la chiusura ha avuto successo
    int sckt_fd = connessioneAlDirettore();
    scrivo_sul_socket(sckt_fd, CHIUSURA_SUPERMERCATO);
    syscall(close(sckt_fd), "close")
    mutex_UNLOCK(&mtx)
}


/* Fa chiudere il supermercato, attendendo che i clienti all'interno terminino gli acquisti */
void chiusuraSupermercato() {
    mutex_LOCK(&mtx)
    accesso_consentito = false;
    // Manda un segnale al thread incaricato della gestione delle entrate per svegliarlo dalla wait
    ec_nzero(pthread_cond_signal(&exit_cond))
    mutex_UNLOCK(&mtx)
    // Attende che termini il thread incaricato della gestione delle entrate
    ec_nzero(pthread_join(entry_tid, NULL))
    // Attende che siano usciti tutti i clienti
    mutex_LOCK(&mtx)
    while (n_clienti_dentro != 0)
        ec_nzero(pthread_cond_wait(&exit_cond, &mtx))
    // Manda un ordine di chiusura a tutte le casse
    for (int i = 0; i < s_market_args.max_casse_aperte; ++i) {
        if (!casse[i].ordinata_chiusura)
            casse[i].ordinata_chiusura = true;
    }
    // Risveglia tutte le casse che attendevano sulla pthread_cond_wait
    while (threads_counter != 0) {
        for (int i = 0; i < s_market_args.max_casse_aperte; ++i) {
            ec_nzero(pthread_cond_signal(&casse[i].cond))
            mutex_UNLOCK(&mtx)
            mutex_LOCK(&mtx)
        }
    }
    // Notifica al direttore che la chiusura ha avuto successo
    int sckt_fd = connessioneAlDirettore();
    scrivo_sul_socket(sckt_fd, CHIUSURA_SUPERMERCATO);
    syscall(close(sckt_fd), "close")
    mutex_UNLOCK(&mtx)
}


/* Crea un file di log contenente tutte le statistiche della giornata lavorativa */
void scriviLogfile() {
    FILE *log_fp = NULL;
    if ((log_fp = fopen(s_market_args.log_fname, "w")) == NULL) error("fopen")
    // Scrive il numero totale di clienti e il numero totale di prodotti acquistati
    fprintf(log_fp, "%d\n", tot_clienti);
    fprintf(log_fp, "%d\n", tot_prodotti);
    // Scrive tutte le statistiche relative ai clienti
    cliente_t *client;
    for (int i = 0; i < tot_clienti; ++i) {
        client = pop(coda_clienti_usciti);
        if (client == NULL) error_exit()
        fprintf(log_fp, "%d\n", client->index);
        fprintf(log_fp, "%u\n", (unsigned int) client->tid);
        fprintf(log_fp, "%d\n", client->n_prodotti_acquistati);
        fprintf(log_fp, "%f\n", (float) client->tempo_permanenza / 1000);
        fprintf(log_fp, "%f\n", (float) client->tempo_attesa_coda / 1000);
        fprintf(log_fp, "%d\n", client->n_code);
        free(client);
    }
    node_t *curr = NULL;
    int *tmp;
    // Scrive tutte le statistiche relative alle casse
    for (int i = 0; i < s_market_args.max_casse_aperte; ++i) {
        fprintf(log_fp, "%d\n", casse[i].num_cassa);
        fprintf(log_fp, "%d\n", casse[i].n_prod_elaborati);
        fprintf(log_fp, "%d\n", casse[i].n_clienti_serviti);
        curr = casse[i].tempi_aperture->head;
        if (curr == NULL)
            fprintf(log_fp, "0\n");
        else {
            while (curr != NULL) {
                tmp = curr->data;
                if (curr != casse[i].tempi_aperture->head)
                    fprintf(log_fp, " + ");
                fprintf(log_fp, "%f", (float) *tmp / 1000);
                curr = curr->next;
            }
            fprintf(log_fp, "\n");
        }
        curr = casse[i].tempo_per_ogni_cliente->head;
        if (curr == NULL)
            fprintf(log_fp, "0\n");
        else {
            while (curr != NULL) {
                tmp = curr->data;
                if (curr != casse[i].tempo_per_ogni_cliente->head)
                    fprintf(log_fp, " + ");
                fprintf(log_fp, "%f", (float) *tmp / 1000);
                curr = curr->next;
            }
            fprintf(log_fp, "\n");
        }
        fprintf(log_fp, "%d\n", casse[i].n_chiusure);

    }
    ec_nzero(fclose(log_fp))
}

/* Libera la memoria allocata dinamicamente non ancora liberata */
void cleanup() {
    free(s_market_args.log_fname);
    for (int i = 0; i < s_market_args.max_casse_aperte; ++i) {
        delete_queue(casse[i].tempo_per_ogni_cliente);
        delete_queue(casse[i].tempi_aperture);
        ec_nzero(pthread_cond_destroy(&casse[i].cond))
    }
    free(casse);
    delete_queue(coda_clienti_usciti);
}

//cassiere

/* Thread creato da cassiere() incaricato di aggiornare il direttore ogni tot tempo sul numero di clienti in coda */
void* aggiorna_direttore(void* arg) {
    int *num_cassa = arg, received_msg;
    struct timespec time;
    // Finché non arriva un ordine di chiusura alla cassa e l'ingresso al supermercato è aperto
    while (!casse[*num_cassa].ordinata_chiusura && !chiusura_immediata && accesso_consentito) {
        // Attende il tempo descritto nel file di configurazione
        time.tv_sec = 0;
        time.tv_nsec = s_market_args.intrv_resoconto_director * 1000000;
        while (nanosleep(&time, &time));
        mutex_LOCK(&mtx)
        // Se intanto è stata ordinata la chiusura oppure è stato chiuso l'ingresso smette di informare il direttore
        if (casse[*num_cassa].ordinata_chiusura || chiusura_immediata || !accesso_consentito) {
            mutex_UNLOCK(&mtx)
            break;
        }
        /* Inizia aggiornamento al direttore */
        // Si connette via socket al direttore
        int dc_fd = connessioneAlDirettore();
        // Invia la richiesta di aggiornamento
        scrivo_sul_socket(dc_fd, RICHIESTA_NUMERO_CLIENTI);
        // Riceve la richiesta di inviare il numero della cassa
        received_msg = leggo_dal_socket(dc_fd);
        if (received_msg != RICHIESTA_NUMERO_CASSA) error_exit()
        // Invia il numero della cassa
        scrivo_sul_socket(dc_fd, *num_cassa);
        // Aspetta conferma di lettura
        received_msg = leggo_dal_socket(dc_fd);
        // Il direttore richiede il numero di clienti in coda
        if (received_msg != RICHIESTA_TOTALE_CLIENTI_IN_CODA) error_exit()
        // Invia al direttore il numero di clienti in coda alla cassa
        scrivo_sul_socket(dc_fd, casse[*num_cassa].clienti_in_coda);
        // Attende la conferma di lettura
        received_msg = leggo_dal_socket(dc_fd);
        // Aggiornamento completato
        if (received_msg != AGGIORNAMENTO_COMPLETATO) error_exit()
        syscall(close(dc_fd), "close")
        mutex_UNLOCK(&mtx)
    }
    return NULL;
}


/* Aux function di cassiere(): chiude la cassa facendo uscire tutti i clienti dalla coda e cancellandola */
void chiusura_cassa(int num_cassa) {
    cliente_t *client;
    // Fa uscire tutti i clienti dalla coda
    while (casse[num_cassa].clienti_in_coda != 0) {
        client = pop(casse[num_cassa].coda);
        if (client == NULL) error("pop")
        client->n_cassa_cliente = -1;
        casse[num_cassa].clienti_in_coda--;
    }
    // Cancella la coda
    delete_queue(casse[num_cassa].coda);
}


/* Thread cassiere */
void* cassiere(void* arg) {
    int *num_cassa = arg;
    int tempo_base, tempo_prodotti, *tempo_servizio = NULL;
    // Crea un thread incaricato di aggiornare il direttore sul numero di clienti in coda
    pthread_t inf_tid;
    ec_nzero(pthread_create(&inf_tid, NULL, aggiorna_direttore, num_cassa))
    threads_counter++;
    // Generazione del tempo base per il servizio di ogni cliente
    unsigned int seed = (unsigned int) casse[*num_cassa].tid;
    tempo_base = 20 + (rand_r(&seed) % 61);      // da 20 a 80 millisec
    struct timespec time;
    cliente_t *client;
    long start = tempoInMillisecondi();
    // Finché non è stata ordinata la chiusura della cassa e non è in corso una evacuazione
    while (!casse[*num_cassa].ordinata_chiusura && !chiusura_immediata) {
        mutex_LOCK(&mtx)
        // Se è stata ordinata la chiusura prima dell'acquisizione del mutex allora esce dal ciclo
        if (casse[*num_cassa].ordinata_chiusura || chiusura_immediata) {
            mutex_UNLOCK(&mtx)
            break;
        }
        // Se ci sono 0 clienti in coda aspetta
        while (!casse[*num_cassa].ordinata_chiusura && casse[*num_cassa].clienti_in_coda == 0 && !chiusura_immediata)
            ec_nzero(pthread_cond_wait(&casse[*num_cassa].cond, &mtx))
        // Se è uscito dal ciclo perché la cassa deve chiudere allora esce
        if (casse[*num_cassa].ordinata_chiusura || chiusura_immediata) {
            mutex_UNLOCK(&mtx)
            break;
        }
        /* Inizia a servire un cliente */
        // Estrae un dato dalla coda (cliente da servire)
        client = pop(casse[*num_cassa].coda);
        if (client == NULL) error("pop")
        // Calcolo del tempo che ci metterà a gestire gli articoli
        tempo_prodotti = s_market_args.tempo_cassiere_singolo_prodotto * client->n_articoli_scelti;
        // Calcolo del tempo totale che ci impiegherà a servire il cliente
        calloc_(tempo_servizio, 1, int)
        *tempo_servizio = tempo_base + tempo_prodotti;
        // Serve il cliente
        time.tv_sec = 0;
        time.tv_nsec = *tempo_servizio * 1000000;
        while (nanosleep(&time, &time));
        // Cliente servito, aggiornamento delle statistiche
        client->servito = true;
        client->n_prodotti_acquistati = client->n_articoli_scelti;
        tot_prodotti += client->n_prodotti_acquistati;
        casse[*num_cassa].n_prod_elaborati += client->n_prodotti_acquistati;
        casse[*num_cassa].n_clienti_serviti++;
        casse[*num_cassa].clienti_in_coda--;
        int err = push(casse[*num_cassa].tempo_per_ogni_cliente, tempo_servizio);
        if (err) error("push")
        mutex_UNLOCK(&mtx)
    }
    /* Ordinata chiusura della cassa */
    long end = tempoInMillisecondi();
    unsigned long *tempo_apertura = NULL;
    calloc_(tempo_apertura, 1, unsigned long)
    *tempo_apertura = end - start;
    // Attesa che termini il thread incaricato di aggiornare il direttore
    ec_nzero(pthread_join(inf_tid, NULL))
    // Aggiornamento delle statistiche
    mutex_LOCK(&mtx)
    chiusura_cassa(*num_cassa);
    int err = push(casse[*num_cassa].tempi_aperture, tempo_apertura);
    tempo_apertura = NULL;
    if (err) error("push")
    casse[*num_cassa].n_chiusure++;
    casse[*num_cassa].aperta = false;
    n_casse_aperte--;
    threads_counter--;
    mutex_UNLOCK(&mtx)
    free(tempo_apertura);
    return NULL;
}


/* Aux function di threadOperaio(): apre una cassa */
void apertura_cassa(int num_cassa) {
    casse[num_cassa].aperta = true;
    casse[num_cassa].ordinata_chiusura = false;
    casse[num_cassa].clienti_in_coda = 0;
    casse[num_cassa].coda = init_queue();
    if (casse[num_cassa].coda == NULL) error("init_queue")
    // Creazione del thread cassiere
    pthread_attr_t attr;
    ec_nzero(pthread_attr_init(&attr))
    ec_nzero(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
    ec_nzero(pthread_create(&casse[num_cassa].tid, &attr, cassiere, &casse[num_cassa].num_cassa))
    n_casse_aperte++;
}



//CLIENTE


/* Aux function di cliente(): il cliente sceglie i prodotti da acquistare */
static void scelta_prodotti(cliente_t *client) {
    unsigned int seed = (unsigned int) client->tid;
    int tempo_di_acquisto = 10 + (rand_r(&seed) % (s_market_args.tempo_acquisti_cliente - 9));
    struct timespec time;
    time.tv_sec = 0;
    time.tv_nsec = tempo_di_acquisto * 1000000;
    while (nanosleep(&time, &time));
    // Generazione random del numero di articoli scelti
    client->n_articoli_scelti = rand_r(&seed) % (s_market_args.max_prodotti_cliente + 1);
}


/* Aux function di cliente(): il cliente ha scelto 0 articoli e quindi manda la richiesta di uscita al direttore */
static void uscita_senza_acquisti(cliente_t *client) {
    // Acquisisco il mutex da prima della connessione, altrimenti potrebbe capitare che il thread che acquisisce il
    // mutex non sia lo stesso che si aspetta il thread worker che ha acquisito il mutex nel processo director
    mutex_LOCK(&mtx)
    // Connessione al processo director
    int dc_fd = connessioneAlDirettore();
    // Manda la richiesta di uscita
    scrivo_sul_socket(dc_fd, RICHIESTA_USCITA_SENZA_ACQUISTI);
    // Attende la ricezione della richiesta
    int received_msg = leggo_dal_socket(dc_fd);
    if (received_msg != RICHIESTA_NUMERO_PRODOTTI) error_exit()
    // Manda il numero di prodotti scelti (ovvero 0)
    scrivo_sul_socket(dc_fd, client->n_articoli_scelti);
    // Attende la risposta dal direttore
    int permission = leggo_dal_socket(dc_fd);
    if (permission != PERMESSO_CONCESSO) error_exit()
    syscall(close(dc_fd), "close")
    mutex_UNLOCK(&mtx)
}


/* Aux function di cliente(): il cliente si mette in coda ad una cassa aperta */
static void in_coda(cliente_t *client) {
    unsigned int seed = (unsigned int) client->tid;
    mutex_LOCK(&mtx)
    // Se è in corso una evacuazione esce
    if (chiusura_immediata || !n_casse_aperte) {
        mutex_UNLOCK(&mtx)
        return;
    }
    int ind_cassa = 1 + (rand_r(&seed) % n_casse_aperte);
    int num_cassa = 0;
    // Sceglie la cassa in cui mettersi in coda
    while (ind_cassa > 0 && num_cassa < s_market_args.max_casse_aperte) {
        if (casse[num_cassa].aperta)
            ind_cassa--;
        if (ind_cassa > 0)
            num_cassa++;
    }
    // Si mette in coda
    client->n_cassa_cliente = num_cassa;
    client->n_code++;
    casse[num_cassa].clienti_in_coda++;
    int err = push(casse[num_cassa].coda, client);
    if (err) error("push")
    // Nel caso la cassa fosse in attesa di clienti si risveglia dalla pthread_cond_wait
    ec_nzero(pthread_cond_signal(&casse[num_cassa].cond))
    mutex_UNLOCK(&mtx)
}



/*
 * Aux function di cliente(): ogni tot tempo il cliente decide se cambiare cassa sulla base del numero di persone
 * in coda davanti a lui, e su quelle in coda alle altre casse. Se esce da una coda e quella scelta nel frattempo viene
 * chiusa, allora ne sceglie un'altra.
 */
static void cambio_coda(cliente_t *client) {
    struct timespec time;
    int choice, min_clienti;
    while (!client->servito && !chiusura_immediata) {
        // Attende che passi il tempo per la decisione, descritto nel file di configurazione
        time.tv_sec = 0;
        time.tv_nsec = s_market_args.tempo_decisione_cliente * 1000000;
        while (nanosleep(&time, &time));
        mutex_LOCK(&mtx)
        // Se nel frattempo è stato servito oppure è in corso una evacuazione esce
        if (client->servito || chiusura_immediata) {
            mutex_UNLOCK(&mtx)
            break;
        }
        /* Inizia la ricerca della nuova cassa */
        if (client->n_cassa_cliente == -1) {    // Se la cassa è stata chiusa mentre era in coda (non è in coda)
            min_clienti = s_market_args.max_clienti + 1;    // +Inf
            choice = -1;
        } else {
            // Conta i clienti davanti
            min_clienti = nodes_to_head(casse[client->n_cassa_cliente].coda, client);
            if (min_clienti == -1) error("nodes_to_head")
            // Nel caso non trovasse niente di meglio rimarrebbe nella stessa coda
            choice = client->n_cassa_cliente;
        }
        // Inizia la decisione
        for (int i = 0; i < s_market_args.max_casse_aperte; ++i) {
            // Se c'è una cassa aperta con meno clienti di quelli davanti a lui la sceglie (le controlla tutte
            // e sceglie quella con meno clienti in assoluto)
            if (casse[i].aperta && casse[i].clienti_in_coda < min_clienti) {
                choice = i;
                min_clienti = casse[i].clienti_in_coda;
            }
        }
        if (choice == -1) error_exit()
        // Se ha deciso di rimanere nella stessa cassa ripete il ciclo
        if (choice == client->n_cassa_cliente || chiusura_immediata) {
            mutex_UNLOCK(&mtx)
            continue;
        }
        // Se il cliente si trova in una coda allora ne esce
        if (client->n_cassa_cliente != -1) {
            int err = remove_from_the_middle(casse[client->n_cassa_cliente].coda, client);
            if (err) error("remove_from_the_middle")
            casse[client->n_cassa_cliente].clienti_in_coda--;
            client->n_cassa_cliente = -1;
        }
        mutex_UNLOCK(&mtx)
        /*
         * Cliente uscito dalla coda --> se la cassa scelta è ancora aperta ci va, altrimenti deve sceglierne un'altra
         * nota che non può essere stato servito perché aveva acquisito il mutex ed è uscito dalla coda mentre era
         * ancora locked
         */
        mutex_LOCK(&mtx)
        if (chiusura_immediata) {
            mutex_UNLOCK(&mtx)
            break;
        }
        // Se la coda nel frattempo è stata chiusa ne deve scegliere un'altra
        if (!casse[choice].aperta) {
            min_clienti = s_market_args.max_clienti + 1;     // +Inf
            choice = -1;
            for (int i = 0; i < s_market_args.max_casse_aperte; ++i) {
                if (casse[i].aperta && casse[i].clienti_in_coda < min_clienti) {
                    choice = i;
                    min_clienti = casse[i].clienti_in_coda;
                }
            }
            if (choice == -1) error_exit()
        }
        // La cassa scelta adesso è sicuramente aperta, quindi si mette in coda
        int err = push(casse[choice].coda, client);
        if (err) error("push")
        // Aggiornamento della cassa in cui si trova il cliente
        client->n_cassa_cliente = choice;
        client->n_code++;
        casse[choice].clienti_in_coda++;
        // Nel caso la cassa fosse in attesa di clienti si risveglia dalla pthread_cond_wait
        ec_nzero(pthread_cond_signal(&casse[choice].cond))
        mutex_UNLOCK(&mtx)
    }
}




/* Thread cliente */
void* customer(void* arg) {
    cliente_t *client = arg;
    // Inizializzo i parametri
    mutex_LOCK(&mtx)
    client->index = tot_clienti;
    tot_clienti++;
    threads_counter++;
    mutex_UNLOCK(&mtx)
    client->n_cassa_cliente = -1;
    client->n_code = 0;
    client->tempo_attesa_coda = 0;
    client->tempo_permanenza = 0;
    client->n_prodotti_acquistati = 0;
    client->n_articoli_scelti = 0;
    client->servito = false;
    long start_coda = 0, end_coda = 0;
    long start = tempoInMillisecondi();
    /* Il cliente inizia gli acquisti */
    // Sceglie gli articoli da acquistare
    scelta_prodotti(client);
    // Se non ha scelto nessun articolo richiede l'autorizzazione per uscire al direttore
    if (client->n_articoli_scelti == 0 && !chiusura_immediata)
        uscita_senza_acquisti(client);
        // Altrimenti si mette in coda ad una cassa ed ogni tot tempo sceglie se cambiare coda
    else if (!chiusura_immediata) {
        in_coda(client);
        start_coda = tempoInMillisecondi();
        cambio_coda(client);
        end_coda = tempoInMillisecondi();
    }
    // Il cliente è stato servito ed esce dal supermercato
    long end = tempoInMillisecondi();
    client->tempo_permanenza = end - start;
    client->tempo_attesa_coda = end_coda - start_coda;
    mutex_LOCK(&mtx)
    int err = push(coda_clienti_usciti, client);
    if (err) error_exit()
    n_clienti_dentro--;
    threads_counter--;
    ec_nzero(pthread_cond_signal(&exit_cond))
    mutex_UNLOCK(&mtx)
    return NULL;
}