#!/bin/bash

if !(ls | grep -q $1); then
	echo "[ERROR] $0: File $1 not found" 1>&2
	exit 1
fi

exec 3<$1

echo ""

# Leggo clienti totali dal file
read -u 3 line
tot_clienti=$line
echo "Clienti totali: $tot_clienti"
# Leggo prodotti totali dal file
read -u 3 line
tot_prodotti=$line
echo "Prodotti totali: $tot_prodotti"

# Leggo le statistiche relative ai clienti
echo ""
echo "Statistiche clienti:"
echo ""
awk 'BEGIN{ printf "|%5s| |%11s| |%7s| |%8s| |%8s| |%6s|\n","id", "tid", "n_prod", "t_perm", "t_coda", "n_code" }'
for ((i=0; i<$tot_clienti; ++i)); do
	read -u 3 line
	indice=$line
	read -u 3 line
	tid=$line
	read -u 3 line
	prod_acquistati=$line
	read -u 3 line
	t_perm=$line
	read -u 3 line
	t_attesa_coda=$line
	read -u 3 line
	n_code=$line
	#stampa
	echo $indice $tid $prod_acquistati $t_perm $t_attesa_coda $n_code\
        | awk '{ printf "|%5s| |%11s| |%7s| |%8.3f| |%8.3f| |%6s|\n", $1, $2, $3, $4, $5, $6 }' 
done

# Leggo le statistiche relative alle casse
echo ""
echo "Statistiche casse:"
echo ""
awk 'BEGIN{ printf "|%5s| |%7s| |%8s| |%10s| |%10s| |%7s|\n","num", "n_prod", "n_client", "tot_apert", "media_ser", "n_chius" }'
while read -u 3 line; do
	#numero cassa
	num_cassa=$line
	#numero di articoli elaborati
	read -u 3 line
	n_prodotti=$line
	#numero di clienti serviti
	read -u 3 line
	n_clienti=$line
	#totale aperture
	read -u 3 line
	tot_apertura=$(echo "scale=6;$line" | bc)
	#tempo medio di servizio di un cliente
	read -u 3 line
	sum=$(echo "scale=6;$line" | bc)
	if [ $n_clienti -gt 0 ]; then
		media_servizi=$(echo "scale=6;$sum/$n_clienti" | bc)
	else media_servizi=0
	fi
	#numero di chiusure
	read -u 3 line
	n_chiusure=$line
	#stampa
	echo $num_cassa $n_prodotti $n_clienti $tot_apertura $media_servizi $n_chiusure\
		| awk '{ printf "|%5s| |%7s| |%8s| |%10.3f| |%10.3f| |%7s|\n", $1, $2, $3, $4, $5, $6 }'
done

