from nose.tools import make_decorator, TimeExpired
from threading import Thread, Timer
from subprocess import Popen
from unittest import TestCase
import unittest
import thread
import signal
import datetime
import os
import logging
import time

log = logging.getLogger('nose.plugins.timeoutPlugin')

def raiseTimeout():
    pass

def timed(limit, handler = raiseTimeout):
    """Test must finish within specified time limit to pass.

    Example use::

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
                timer.cancel()
                log.debug('canceled timer in %s because of failure', str(datetime.datetime.now()))
                raise
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
    time.sleep(1)

handlerUsed = []
def writeLocalHandler(h = handlerUsed):
    h.append('i')
    
@timed(0.1, writeLocalHandler)
def handledSleeper():
    time.sleep(1)

@timed(5)
def passer():
    time.sleep(1)
    
class TimeoutTest(TestCase):    

    def testPass(self):
        try:
            passer()
        except TimeExpired:
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
