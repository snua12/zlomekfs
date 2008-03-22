#!/bin/bash

WORKER_PIDFILE=/var/run/infiniteWorker.pid
CONTROL_PIDFILE=/var/run/infiniteControl.pid

LOCKDIR=/tmp/insecticide.lockdir

echo $$ > ${CONTROL_PIDFILE}

if [ "$1" == "start" ]; then
    rmdir "${LOCKDIR}"
    while true; do
        while [ -d "${LOCKDIR}" ]; do
            sleep 1
        done
        
        svn up 
        
        PROFILE_NAME=profile_infinite ./test.py testStressFSOp.py &
        echo $! > "${WORKER_PIDFILE}"
        while [ ! -d "${LOCKDIR}" ]; do
            sleep 1
        done
        kill -2 `cat ${WORKER_PIDFILE}`
    done

elif [ "$1" == "stop" ]; then

    mkdir -p "${LOCKDIR}"

    kill -SIGINT `cat ${WORKER_PIDFILE}`
    kill -SIGINT `cat ${CONTROL_PIDFILE}`
    sleep 5
    kill -9 `cat ${CONTROL_PIDFILE}`
    
    wait `cat ${CONTROL_PIDFILE}` `cat ${WORKER_PIDFILE}`

    rmdir "${LOCKDIR}"

elif [ "$1" == "pause" ]; then

    mkdir -p "${LOCKDIR}"
    kill -SIGINT `cat ${WORKER_PIDFILE}`
    wait `cat ${WORKER_PIDFILE}`

elif [ "$1" == "unpause" ]; then

    rmdir "${LOCKDIR}"

else
   echo "unknown command"
fi
