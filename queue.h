#ifndef PROJECTSO_QUEUE_H
#define PROJECTSO_QUEUE_H

#include "shared.h"


/** Elemento della coda */
typedef struct node {
    void* data;
    struct node *next;
} node_t;


/** Struttura dati coda */
typedef struct queue {
    node_t *head;
    node_t *tail;
    size_t length;
} queue_t;


/**
 * Alloca ed inizializza una coda.
 * @return NULL in caso di fallimento (setta errno).
 * @return puntatore alla coda allocata in caso di successo.
 */
queue_t* init_queue();

/**
 * Cancella una coda. In caso di coda vuota non fa niente (e setta errno).
 * @param queue puntatore alla coda da cancellare.
 */
void delete_queue(queue_t *queue);

/**
 * Inserisce un dato in fondo alla coda.
 * @param data puntatore al dato da inserire.
 * @return 0 in caso di successo.
 * @return 1 in caso di fallimento (setta errno).
 */
int push(queue_t *queue, void* data);

/**
 * Estrae un dato dalla cima della coda.
 * @param queue puntatore alla coda.
 * @return NULL in caso di fallimento (setta errno).
 * @return il dato estratto in caso di successo.
 */
void* pop(queue_t *queue);

/**
 * Estrae un dato da un qualsiasi punto della coda
 * @param queue puntatore alla coda
 * @param data il dato da rimuovere
 * @return 0 in caso di successo
 * @return 1 in caso di errore (setta errno)
 */
int remove_from_the_middle(queue_t *queue, void* data);

/**
 * Conta il numero di nodi che separano il nodo contenente data dalla testa della coda
 * @param queue puntatore alla coda
 * @param data il dato da cui iniziare a contare
 * @return il numero di nodi da data alla testa della coda a data
 * @return -1 in caso di errore (setta errno)
 */
size_t nodes_to_head(queue_t *queue, void* data);



#endif //PROJECTSO_QUEUE_H
