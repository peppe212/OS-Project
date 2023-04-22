#ifndef PROJECTSO_CLIENT_H
#define PROJECTSO_CLIENT_H

#include "queue.h"


// Struttura dati per memorizzare i parametri del file di configurazione
typedef struct supermarket_args {
    int max_casse_aperte;           // Massimo di casse aperte contemporaneamente
    int max_clienti;                // Massimo di clienti nel supermercato
    int soglia_clienti;             // Una volta usciti soglia_clienti clienti ne entrano altri soglia_clienti
    int tempo_decisione_cliente;    // Tempo in millisec ogni qual volta un cliente in coda sceglie se cambiare cassa
    int tempo_acquisti_cliente;     // Tempo massimo (> 10 millisec) che ci mette un cliente a scegliere i prodotti
    int max_prodotti_cliente;       // Numero massimo di prodotti acquistabili da un cliente
    int n_casse_apertura;           // Numero di cassa aperte all'apertura del supermercato
    char *log_fname;                // Nome del file di log
    int tempo_cassiere_singolo_prodotto; // Tempo in millisec che un cassiere impiega per gestire un singolo prodotto
    int intrv_resoconto_director;   // Tempo in millisec ogniqualvolta ciascun cassiere manda un resoconto al direttore
} supermarket_args_t;


// Struttura dati per memorizzare i dati relativi alle casse
typedef struct cassa {
    pthread_t tid;
    int num_cassa;
    bool aperta;
    bool ordinata_chiusura;
    int clienti_in_coda;
    int n_clienti_serviti;
    int n_prod_elaborati;
    int n_chiusure;
    queue_t* coda;
    queue_t* tempi_aperture;
    queue_t* tempo_per_ogni_cliente;
    pthread_cond_t cond;
} cassa_t;


// Struttura dati per memorizzare i dati relativi ai clienti
typedef struct cliente {
    int index;
    pthread_t tid;
    bool servito;
    int n_articoli_scelti;
    int n_cassa_cliente;
    unsigned long tempo_permanenza;
    unsigned long tempo_attesa_coda;
    int n_code;
    int n_prodotti_acquistati;
} cliente_t;


/**
 * Libera la memoria allocata dinamicamente. Viene chiamata alla normale terminazione del programma.
 */
void cleanup();

/**
 * Gestione dei segnali del processo supermarket. Maschera i segnali SIGHUP, SIGQUIT e SIGPROF e ignora il segnale
 * SIGPIPE.
 */
void gestoreSegnaliMarket();

/**
 * Si connette con un socket al director.
 * @return il file descriptor utilizzato dal processo supermarket per la comunicazione
 */
int connessioneAlDirettore();

/**
 * Legge i parametri dal file di configurazione.
 * @param config_fname
 */
void parsingMarket(char* config_fname);

/**
 * Apre il supermercato alla clientela, facendo entrare il numero massimo di clienti ed aprendo il numero
 * di casse descritto nel file di configurazione. Inoltre crea un thread incaricato di far entrare i clienti
 * ogni volta che ne esce la soglia descritta nel file di configurazione.
 */
void aperturaSupermercato();

/**
 * Thread incaricato di ricevere gli ordini di apertura o chiusura di una cassa e di eseguirli.
 * @param arg
 * @return NULL
 */
void* threadOperaio(void* arg);


/**
 * Chiude il supermercato immediatamente senza lasciar terminare gli acquisti ai clienti.
 */
void chiusuraImmediataSupermercato();

/**
 * Chiude il supermercato chiudendo l'entrata e lasciando terminare gli acquisti ai clienti già all'interno.
 */
void chiusuraSupermercato();

/**
 * Crea un file di log contenente tutte le statistiche relative alla giornata lavorativa.
 */
void scriviLogfile();


long tempoInMillisecondi();

void chiusura_cassa(int num_cassa);
void apertura_cassa(int num_cassa);
void* cassiere(void* arg);
void* customer(void* arg);



#endif //PROJECTSO_CLIENT_H
