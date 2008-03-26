#!/bin/bash

WORKER_PIDFILE=/var/run/infiniteWorker.pid
CONTROL_PIDFILE=/var/run/infiniteControl.pid

LOCKDIR=/tmp/insecticide.lockdir

TESTS='testStressFSOp.py testFSOps.py clientServerBaseTest.py'

# interrupted nose could leak some temporary data... this function removes them
function collectGarbage()
{
    rm -rf /tmp/nose* /tmp/zfs* /tmp/insecticide*.log* /tmp/testCompareDir* >/dev/null 2>&1
}

function terminate()
{
    wait
    exit 0
}

function killWithWait()
{
    kill -SIGINT $1 >/dev/null 2>&1
    for i in 1 2 3 5 10 10 10 30; do
        if ! kill -0 $1 >/dev/null 2>&1; then
            break
        fi
        sleep $i
    done
    if kill -0 $1 >/dev/null 2>&1; then
        kill -SIGKILL $1 >/dev/null 2>&1
    fi
}

trap  terminate SIGINT
trap  terminate SIGTERM

if [ "$1" == "run" ]; then
    
    echo $$ > ${CONTROL_PIDFILE}
    rmdir "${LOCKDIR}" >/dev/null 2>&1
    while true; do
        while [ -d "${LOCKDIR}" ]; do
            sleep 1
        done
        
        svn up 
        killWithWait `cat ${WORKER_PIDFILE}`
        PROFILE_NAME=profile_infinite ./test.py $TESTS &
        echo $! > "${WORKER_PIDFILE}"
        while [ ! -d "${LOCKDIR}" ]; do
            sleep 1
            if ! kill -0 `cat ${WORKER_PIDFILE}` >/dev/null 2>&1; then
                break
            fi
        done
	collectGarbage
        #killWithWait `cat ${WORKER_PIDFILE}`
    done
    
elif [ "$1" == "stop" ]; then

    mkdir -p "${LOCKDIR}"

    killWithWait `cat ${WORKER_PIDFILE}`
    killWithWait `cat ${CONTROL_PIDFILE}`
    
    rmdir "${LOCKDIR}"

elif [ "$1" == "pause" ]; then

    mkdir -p "${LOCKDIR}"
    killWithWait `cat ${WORKER_PIDFILE}`

elif [ "$1" == "unpause" ]; then

    rmdir "${LOCKDIR}" >/dev/null 2>&1
    
else
    
   echo "unknown command"
   exit 1
   
fi

exit 0
