"""
    Module with timeout related code. 
    Provides functions and decorators used for timeout handling.
"""

import unittest
import datetime
import logging
import time
import sys

from threading import Timer
from unittest import TestCase

from nose.tools import make_decorator, TimeExpired

log = logging.getLogger('nose.plugins.timeoutPlugin')
""" Logger used within this module code. """

def voidTimeoutHandler():
    """  Empty timeout handler. Does nothing. """
    pass

def timed(limit, handler = voidTimeoutHandler):
    """ Decorator to use to constrain test's run time.
        Test must finish within specified time limit to pass.
        When time runs out, handler function is called, and exception
        TimeExpired raised.
        
        :Parameters:
            timit: time limit in seconds (floats - 0.1 - allowed)
            handler: function to call when timeout is reached
            
        :Return:
            test return value
            
        :Raise:
            TimeExpired exception if time expires
        
        :Example usage:
            
            @timed(.1)
            def test_that_fails():
                time.sleep(.2)
    """
    def decorate(func):
        def newfunc(*arg, **kw):
            timer = Timer(limit, handler)
            log.debug('starting timer in %s for %s', str(datetime.datetime.now()), str(limit))
            timer.start()
            try:
                ret = func(*arg, **kw)
            except:
                if timer.isAlive():
                    timer.cancel()
                    log.debug('canceled timer in %s because of failure', str(datetime.datetime.now()))
                    raise
                else:
                    raise TimeExpired('Time expired ( and test raised: ' \
                        + str(sys.exc_info()) + ')')
            if timer.isAlive():
                timer.cancel()
                log.debug('canceled timer in %s', str(datetime.datetime.now()))
            else:
                log.debug('timer has expired', str(datetime.datetime.now()))
                raise TimeExpired("time limit exceeded")
            return ret
        newfunc = make_decorator(func)(newfunc)
        return newfunc
    return decorate
    

@timed(0.1)
def sleeper():
    """ Function for timeout unittest testing.
        Timeout should be raised.
    """
    time.sleep(1)

handlerUsed = []
""" Array which is used to indicate if handler was run.
    evaluation is made by 
    if handlerUsed:
        #handler have run
"""

def writeLocalHandler(h = handlerUsed):
    """ Handler used in unittest. 
        Indicates it's run by appending into handlerUsed array.
    """
    h.append('i')
    
@timed(0.1, writeLocalHandler)
def handledSleeper():
    """ Function for timeout and handlers unittest testing.
        Timeout should be raised and handler executed.
    """
    time.sleep(1)

@timed(5, writeLocalHandler)
def passer():
    """ Function for timeout and handlers unittest testing.
        Should pass without timeout and handler.
    """
    time.sleep(1)
    
class TimeoutTest(TestCase):    
    """ Class with tests for timed decorator. """
    def testPass(self):
        handlerUsed = []
        try:
            passer()
        except TimeExpired:
            assert False
        if handlerUsed:
            assert False
        assert True
        
    def testDefaultAction(self):
        try:
            sleeper()
        except TimeExpired:
            return
        assert False
        
    def testWriteHandler(self):
        try:
            handledSleeper()
        except TimeExpired:
            if handlerUsed:
                return
        assert False
        
    

if __name__ == '__main__':
    unittest.main()
