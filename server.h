#ifndef PROJECTSO_SERVER_H
#define PROJECTSO_SERVER_H

#include "shared.h"

// Struttura dati per memorizzare i parametri del file di configurazione
typedef struct director_args {
    // Soglia s1 per la quale se ci sono s1 casse con al massimo un cliente, allora chiude una cassa
    int s_chiusura_cassa;
    // Soglia s2 per la quale se c'è almeno una cassa con almeno s2 clienti in coda, allora apre una cassa
    int s_apertura_cassa;
    // Massimo di casse aperte contemporaneamente
    int max_casse_aperte;
} director_args_t;


/**
 * Se esiste, cancella il socket il cui nome è il valore della macro SOCK_NAME (presente nel file macro.h)
 */
void cleanup();

/**
 * Gestione dei segnali. Maschera i segnali SIGHUP, SIGQUIT e SIGPROF, ignora il segnale SIGPIPE e infine crea un
 * thread incaricato di gestire i segnali. Quest'ultimo in particolare, se riceve un segnale SIGHUP o SIGQUIT lo inoltra
 * al processo supermarket, mentre se riceve un segnale SIGPROF significa che il processo supermarket è pronto a ricevere
 * un ordine di apertura o chiusura di una cassa.
 */
void gestoreSegnaliDirettore();

/**
 * Forka il processo director ed esegue una exec per eseguire il processo supermarket
 */
void eseguiSupermercato();

/**
 * Inizia la connessione via socket con il supermercato.
 * @param sock_fd
 * @return file descriptor per la comunicazione
 */
int connessioneAlSupermercato(int *sock_fd);

/**
 * Legge i parametri dal file di configurazione.
 * @param config_fname
 */
void parsingDirettore(char* config_fname);

/**
 * Crea un thread che esegua una specifica richiesta del processo supermarket.
 * @param fd
 */
void creaThreadWorker(int fd);


#endif //PROJECTSO_SERVER_H
