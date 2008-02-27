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
        
    
    
'''    
            block = [os.getppid()]
            def handler(signum, stack):
                if block:
                    raise  TimeExpired("Time limit (%s) exceeded" % limit)
            signal.signal(signal.SIGTERM, handler)
            def fuck_them(block):
                time.sleep(limit)
                if block:
                    os.kill(block[0], signal.SIGTERM)
            thread.start_new_thread(fuck_them, (block,))
            func(*arg, **kw)
            block.pop(0)
        
            block = ['a']
            def handler(signum, stack, block = block):
                if block:
                    raise  TimeExpired("Time limit (%s) exceeded" % limit)
            signal.signal(signal.SIGALRM, handler)
            signal.alarm(limit)
            func(*arg, **kw)
            block.pop(0)

            def inter(limit):
                raise TimeExpired("Time limit (%s) exceeded" % limit)
            timer = Timer(limit, inter, args = [limit])
            timer.start()
            func(*arg, **kw)
            timer.cancel()
            
            runner = Thread(group = None, target = func, name = str(func), *arg, **kw)
            runner.start()
            runner.join(limit)
            if runner.isAlive():
                raise TimeExpired("Time limit (%s) exceeded" % limit)
                
            def inter(time, flag):
                time.sleep(time)
                if flag:
                    thread.interrupt_main()
                thread.exit()
            flag = [1]
            thread.start_new_thread(intern, (limit, flag))
            try:
                func(*arg, **kw)
                flag.pop(0)
            except KeyboardInterrupt:
                raise TimeExpired("Time limit (%s) exceeded" % limit)

'''

if __name__ == '__main__':
    unittest.main()
