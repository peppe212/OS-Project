#ifndef PROJECTSO_SHARED_H
#define PROJECTSO_SHARED_H


#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <sys/time.h>
#include <arpa/inet.h>


#define DEFAULT_CONF_FILE "./config1.txt"
#define MRKT_PATH "./client"
#define SOCK_NAME "./mysock"
#define UNIX_PATH_MAX 108
#define TIMEOUT 20000    // sono 20000 microsecondi che corrispondono a 20 millisecondi
                         // e' il timer che uso nella select del server



#define error(str)  \
    {   \
        fprintf(stderr, "LINE: %d FILE: %s --> ", __LINE__, __FILE__); \
        perror(#str);   \
        fflush(stderr); \
        exit(EXIT_FAILURE);    \
    }


#define syscall(value, str) \
    if ((value) == -1) error(str)


#define error_exit()   \
    {   \
        fprintf(stderr, "[error]    LINE: %d    FILE %s", __LINE__, __FILE__);    \
        exit(EXIT_FAILURE); \
    }


#define ec_nzero(value) \
    if((value) != 0) error_exit()


#define malloc_(obj, size, type)   \
    {   \
        (obj) = malloc((size) * sizeof(type));  \
        if ((obj) == NULL) error("malloc") \
    }


#define calloc_(obj, size, type) \
    {   \
        (obj) = calloc(size, sizeof(type));   \
        if ((obj) == NULL) error("calloc");   \
    }


#define mutex_LOCK(mtx)   \
    if (pthread_mutex_lock(mtx) != 0) error_exit()


#define mutex_UNLOCK(mtx)   \
    if (pthread_mutex_unlock(mtx) != 0) error_exit()



typedef enum messaggi_ {
    RICHIESTA_NUMERO_CLIENTI,
    RICHIESTA_USCITA_SENZA_ACQUISTI,
    RICHIESTA_NUMERO_CASSA,
    RICHIESTA_NUMERO_PRODOTTI,
    PERMESSO_CONCESSO,
    PERMESSO_NEGATO,
    RICHIESTA_TOTALE_CLIENTI_IN_CODA,
    AGGIORNAMENTO_COMPLETATO,
    ORDINE_CHIUSURA_CASSA,
    ORDINE_APERTURA_CASSA,
    RICHIESTA_NOME_FILE_CONFIGURAZIONE,
    APERTURA_CASSA_COMPLETATA,
    CHIUSURA_CASSA_COMPLETATA,
    APERTURA_CASSA_FALLITA,
    CHIUSURA_CASSA_FALLITA,
    CHIUSURA_SUPERMERCATO
} messaggi;


/**
 * Per scrivere su un socket.
 * @param fd
 * @param buffer
 * @param size
 * @return 0 in caso di successo, -1 in caso di fallimento
 */
int writen(int fd, void *buffer, size_t size);

/**
 * Per leggere da un socket.
 * @param fd
 * @param buffer
 * @param size
 * @return n >= 0 bytes letti in caso di successo, -1 in caso di fallimento
 */
int readn(int fd, void *buffer, size_t size);

/**
 * Per scrivere interi su un socket.
 * @param fd file descriptor
 * @param msg intero da inviare
 */
void scrivo_sul_socket(int fd, int msg);

/**
 * Per leggere interi da un socket.
 * @param fd file descriptor
 * @return il numero intero letto
 */
int leggo_dal_socket(int fd);



#endif //PROJECTSO_SHARED_H
