#!/usr/bin/env python


from insecticide.util import noseWrapper


from signal import signal, SIGINT

def raiseKeyboardInterrupt(sig, stack):
    raise KeyboardInterrupt()#, None, extract_stack(stack)
    
# this is needed to enable SIGINT even in daemon mode
signal(SIGINT, raiseKeyboardInterrupt)

noseWrapper(project = 'zlomekfs', stripPath = 'tests/nose-tests')
