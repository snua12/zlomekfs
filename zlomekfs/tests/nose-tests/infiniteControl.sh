#!/bin/bash

WORKER_PIDFILE=/var/run/infiniteWorker.pid
CONTROL_PIDFILE=/var/run/infiniteControl.pid

LOCKDIR=/tmp/insecticide.lockdir

function terminate()
{
    wait
    exit 0
}

trap  terminate SIGINT

if [ "$1" == "run" ]; then
    echo $$ > ${CONTROL_PIDFILE}
    rmdir "${LOCKDIR}" >/dev/null 2>&1
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
        kill -2 `cat ${WORKER_PIDFILE}` >/dev/null 2>&1
    done

elif [ "$1" == "stop" ]; then

    mkdir -p "${LOCKDIR}"

    kill -SIGINT `cat ${WORKER_PIDFILE}` >/dev/null 2>&1
    for i in 1 2 3 5; do
        if ! kill -0 `cat ${WORKER_PIDFILE}` >/dev/null 2>&1; then
            break
        fi
        sleep $i
    done
    
#    if kill -0 `cat ${WORKER_PIDFILE}`; then
#        kill -SIGKILL `cat ${WORKER_PIDFILE}`
#    fi
    
    kill -SIGINT `cat ${CONTROL_PIDFILE}` >/dev/null 2>&1
    for i in 1 2 3; do
        sleep $i
    done
    if kill -0 `cat ${CONTROL_PIDFILE}` >/dev/null 2>&1; then
        kill -SIGKILL `cat ${CONTROL_PIDFILE}` >/dev/null 2>&1
    fi

    rmdir "${LOCKDIR}"

elif [ "$1" == "pause" ]; then

    mkdir -p "${LOCKDIR}"
    kill -SIGINT `cat ${WORKER_PIDFILE}` >/dev/null 2>&1

elif [ "$1" == "unpause" ]; then

    rmdir "${LOCKDIR}" >/dev/null 2>&1

else
   echo "unknown command"
fi
