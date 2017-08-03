#!/bin/bash

#Script che si occupa di elimina tutti i file e directory piu vecchi di t minuti all'interno
#della cartella del server di chatty. Se t vale 0 allora vengono stampati solamente i file al suo
#interno

#funzione per mostrare l'help
usage() { echo "Usage: $0 [-p: configuration_file_path <string>] [-t: time < >=0 >] [-h/-help: display_help] " 1>&2; exit 1; }

#variabile utili
TIME=-1
DIRNAME=""
CONF_FILE=""

#nessun parametro
if [ $# -eq 0 ];then
    usage
fi

#acquisisco parametri
while getopts "hp:t:" opt; do
    case "${opt}" in
        t)
            TIME=${OPTARG}
            ;;
        p)
            CONF_FILE=${OPTARG}
            ;;

        h| * )
            usage
            ;;
    esac
done
shift $((OPTIND-1))

#controllo parametri

#variabile per controllare se stiamo lavorando effetivamente con un file di configurazione di chatty
check_conf_file=$(echo ${CONF_FILE} | grep -o "chatty.conf\?")

#controllo validita' configuration file
if [ -z "${CONF_FILE}" ] || ! [ -e "${CONF_FILE}" ] || [ -z $check_conf_file ]; then
    echo "Invalid configuration file" 1>&2;
    usage
fi

#controllo valdiita' minuti
if [ ${TIME} -lt 0 ]; then
    echo "Invalid time" 1>&2;
    usage
fi

#vado a leggere all'interno del mio file di configurazione il path del socket del server
while read -r line
do
    #se e' un commento vado avanti
    if ! [ -z $(echo $line | grep -o "#") ];then
        continue
    fi

    #ora controllo se la riga corrente e' quella che mi interessa,cioe se ha il campo DirName
    check_line=$(echo ${line} | grep -o "DirName")

    #se e' la riga che cercavo,allora la splitto con il delimitatore =
    if ! [ -z "$check_line" ]; then
        #elimino tutti i  white space dalla riga corrente,usando una regexp
        rem_ws=$(echo ${line//[[:blank:]]/})

        #splitto la stringa con il delimitatore =
        IFS='=' read -ra TMP <<< "$rem_ws"

        #il campo cercato si trovera' in posizione 1
        DIRNAME=${TMP[1]}
        break;
    fi

done < "$CONF_FILE"

#se TIME vale 0,allora devo solo stampare tutti i file presenti nella cartella
if [ $TIME -eq 0 ];then
    find $DIRNAME -type f
#altrimenti devo rimuovere tutti i file,piu' vecchi di TIME minuti,ignorando le cartella .
else
    find $DIRNAME -not -path "$DIRNAME" -name "*" -mmin +$TIME -delete
fi

exit 0
