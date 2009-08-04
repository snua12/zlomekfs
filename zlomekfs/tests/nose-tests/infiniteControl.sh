#!/bin/bash

# Change the directory, to script location, in case somebody ran it from outside
cd `echo $0|sed -e 's/\/[^\/]*$/\//'`

# file with pid of worker (nose) thread
WORKER_PIDFILE=/var/run/infiniteWorker.pid
# file with pid of run process
CONTROL_PIDFILE=/var/run/infiniteControl.pid

# tests to execute in infinite loop
TESTS='testStressFSOp.py testFSOps.py clientServerBaseTest.py'

# interrupted nose could leak some temporary data... this function removes them
function collectGarbage()
{
    rm -rf /tmp/nose* /tmp/zfs* /tmp/insecticide*.log* /tmp/testCompareDir* >/dev/null 2>&1
}

# return 0 if infinite loop is (should be) paused
function isPaused()
{
    python -c '
from insecticide.util import isPaused
import sys

if isPaused():
    sys.exit(0)
else:
    sys.exit(1)
'
    return $?
}

# pause infinite loop
function pause()
{
    python -c 'from insecticide.util import signalPause
signalPause()
'
}

# unpause infinite loop
function unpause()
{
    python -c 'from insecticide.util import signalUnpause
signalUnpause()
'
}

# pause infinite loop, wait for termination and kill after timeout
# first arg: PID of worker (nose)
function pauseWithWait()
{
    pause
    
    for i in 1 2 3 5 10 10 10 30 90 240; do
        if ! kill -0 $1 >/dev/null 2>&1; then
            break
        fi
        sleep $i
    done
    if kill -0 $1 >/dev/null 2>&1; then
        kill -SIGKILL $1 >/dev/null 2>&1
    fi
}

# send SIGINT, wait for termination and then send SIGKILL to process
# first arg: PID
function killWithWait()
{
    kill -SIGINT $1 >/dev/null 2>&1
    for i in 1 2 3 5 10 10 10 30 90 240; do
        if ! kill -0 $1 >/dev/null 2>&1; then
            break
        fi
        sleep $i
    done
    if kill -0 $1 >/dev/null 2>&1; then
        kill -SIGKILL $1 >/dev/null 2>&1
    fi
}

# terminate infinite testing (handler for signals within run)
function terminate()
{
    killWithWait `cat ${WORKER_PIDFILE}`
    wait
    exit 0
}

trap  terminate SIGINT
trap  terminate SIGTERM

# main function of infinite loops
function run()
{
    echo $$ > ${CONTROL_PIDFILE}
    unpause
    while true; do
        while isPaused; do
            sleep 1
        done
        
        svn up 
        killWithWait `cat ${WORKER_PIDFILE}`
        collectGarbage
        PROFILE_NAME=profile_infinite ./test.py $TESTS &
        echo $! > "${WORKER_PIDFILE}"
        while ! isPaused; do
            sleep 1
            if ! kill -0 `cat ${WORKER_PIDFILE}` >/dev/null 2>&1; then
                break
            fi
        done
    done
}

if [ "$1" == "run" ]; then
    
    run
    
elif [ "$1" == "stop" ]; then

    pauseWithWait `cat ${WORKER_PIDFILE}`
    killWithWait `cat ${CONTROL_PIDFILE}`
    
    unpause

elif [ "$1" == "pause" ]; then

    pauseWithWait `cat ${WORKER_PIDFILE}`

elif [ "$1" == "unpause" ]; then

    unpause
    
else
    
   echo "unknown command (use run, stop, pause, unpause)"
   exit 1
   
fi

exit 0
