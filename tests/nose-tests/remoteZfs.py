#! /bin/env python

"""
    locally, we wrap every RemoteReference into RemoteObject
"""

from twisted.spread import pb
from twisted.internet import reactor
from twisted.python.failure import Failure
import sys
import os
import logging
import tempfile
from insecticide.snapshot import SnapshotDescription
from insecticide.timeoutPlugin import TimeExpired
from subprocess import Popen
from threading import Condition
from zfs import ZfsProxy

from nose.tools import make_decorator

log = logging.getLogger('nose.tests.clientServerTest')

def wrapException(func):
    """ Decorator to wrap local exception to twisted Failure object.
        
        :Return:
            function test value
            
        :Raise:
            pb.Error wrapped exception (if raised from function
        
        :Example usage:
            
            @wrapException
            def test_that_fails():
                raise Exception()
    """
    def newfunc(*arg, **kwargs):
        ret = None
        try:
            ret = func(*arg, **kwargs)
        except Exception, value:
            (type, value, tb) = sys.exc_info()
            raise pb.Error(str(type) + ':' + str(value))
        return ret
    newfunc = make_decorator(func)(newfunc)
    return newfunc


class RemoteException(Exception):
    pass

def raiseRemoteException(failure):
    tb = failure.getTraceback()
    #log.debug('tb is %s(%d) of type %s'%(str(tb),id(tb),type(tb)))
    raise RemoteException, str(failure.type) + ':' \
        + failure.getErrorMessage() + ' on\n' \
        + str(failure.getTraceback()), failure.tb


LISTEN_PORT = 8007
CHUNK_SIZE = 4096

class RemoteFile(pb.Referenceable):
    def __init__(self, fileName, mode):
        self.fh=open(fileName, mode)
        
    @wrapException
    def remote_write(self, data):
        return self.fh.write(data)
        
    @wrapException
    def remote_read(self, size = None):
        if size:
            return self.fh.read(size)
        else:
            return self.fh.read()
        
    @wrapException
    def remote_getSize(self):
        pos = self.fh.tell()
        self.fh.seek(0,os.SEEK_END)
        size = self.fh.tell()
        self.fh.seek(pos,os.SEEK_SET)
        return size
        
    @wrapException
    def remote_close(self):
        return self.fh.close()
        
    @wrapException
    def remote_seek(self, offset):
        return self.fh.seek(offset, os.SEEK_SET)
        
    @wrapException
    def remote_tell(self):
        return self.fh.tell()
        
    @wrapException
    def remote_getName(self):
        return self.fh.name
        
    @wrapException
    def remote_delete(self):
        os.unlink(self.fh.name)
        
    @wrapException
    def remote_flush(self):
        self.fh.flush()

class RemoteZfs(pb.Referenceable):
    def __init__(self, *arg, **kwargs):
        self.zfs = ZfsProxy(*arg, **kwargs)
        
    @wrapException
    def remote_start(self):
        return self.zfs.runZfs()
    
    @wrapException
    def remote_stop(self):
        return self.zfs.stopZfs()
        
    @wrapException
    def remote_cleanup(self):
        return self.zfs.cleanup()
    
    @wrapException
    def remote_running(self):
        return self.zfs.running()
    
    @wrapException
    def remote_hasDied(self):
        return self.zfs.hasDied()
        
    @wrapException
    def remote_signalAll(self, signal):
        return self.zfs.signalAll(signal)
        
    @wrapException
    def remote_snapshot(self):
        toDir = tempfile.mkdtemp(prefix="noseSnapshot")
        snapshot = SnapshotDescription(toDir)
        self.zfs.snapshot(snapshot)
        (handle,fileName) = tempfile.mkstemp()
        snapshot.pack(fileName)
        snapshot.delete()
        return RemoteFile(fileName, 'r')
        
    @wrapException
    def remote_getMountPoint(self):
        return self.zfs.zfsRoot
        

class RemoteControl(pb.Root):
    @wrapException
    def remote_system(self, cmdLine):
        print 'executing ' + str(cmdLine)
        proc = Popen(args = cmdLine)
        proc.wait()
        return proc.returncode
        
    @wrapException
    def remote_open(self, fileName, mode):
        return RemoteFile(fileName, mode)
        
    @wrapException
    def remote_makedirs(self, dirName):
        try:
            return os.makedirs(dirName)
        except OSError:
            pass
        
    @wrapException
    def remote_delete(self, fileName):
        return os.unlink(fileName)
        
    @wrapException
    def remote_restart(self):
        print 'executing ' + str(sys.argv)
        reactor.stop()
        os.execv(sys.argv[0],sys.argv)
        
    @wrapException
    def remote_getZfs(self, *arg, **kwargs):
         return RemoteZfs(*arg, **kwargs)
         
    
class SimpleRemoteCall(object):
    returncode = None
    returned = False
    
    def signal(self):
        self.cond.acquire()
        self.returned = True
        self.cond.notify()
        self.cond.release()
        
    def errorHandler(self, error):
        self.returncode = error
        self.signal()
        
    def successHandler(self, object):
        self.returncode = object
        self.signal()
        
    def __call__(*arg, **kwargs):
        pass
        
    def __init__(self, remoteReference, *arg, **kwargs):
        self.cond = Condition()
        deref = remoteReference.callRemote(*arg, **kwargs)
        deref.addCallbacks(self.successHandler, self.errorHandler)
        
    def wait(self, timeout = None):
        self.cond.acquire()
        if not self.returned:
            self.cond.wait(timeout)
            
        self.cond.release()
        if not self.returned:
            raise TimeExpired('timeout')
    
class GetRootObject(SimpleRemoteCall):
    def __init__(self, reactor, host = 'localhost', port = LISTEN_PORT):
        self.cond = Condition()
        
        factory = pb.PBClientFactory()
        reactor.connectTCP(host, port, factory)
        
        deref = factory.getRootObject()
        deref.addCallbacks(getattr(self,'successHandler'), getattr(self, 'errorHandler'))
        

class ReactorWrapper(object):
    def __init__(self, reactor, reactorThread, timeout = 10):
        self.reactor = reactor
        self.reactorThread = reactorThread
        self.timeout = timeout
        
    def setTimeout(self, timeout):
        self.timeout = timeout
        
    def call(self, remoteReference, *arg, **kwargs):
        call = SimpleRemoteCall(remoteReference, *arg, **kwargs)
        
        self.reactor.callFromThread(call)
        #call = cls(*arg, **kwargs)
        call.wait(timeout = self.timeout)
        
        if isinstance(call.returncode, Failure):
	    raiseRemoteException(call.returncode)
        else:
            return call.returncode
        
    def getRemoteObject(self, remoteReference, *args, **kwargs):
        ret = self.call(remoteReference, *args, **kwargs)
        
        if not isinstance(ret, pb.RemoteReference):
            raise TypeError('Invalid return value of type %s', str(type(ret)))
        elif isinstance(ret, Failure):
            raiseRemoteException(ret)
        else:
            return RemoteObjectWrapper(self, ret)
        

class RemoteObjectWrapper(object):
    def __init__(self, reactorWrapper, remoteReference):
        self.remoteReference = remoteReference
        self.reactorWrapper = reactorWrapper
        
    def call(self, *args, **kwargs):
        return self.reactorWrapper.call(self.remoteReference, *args, **kwargs)
        
    def getRemoteObject(self, *args, **kwargs):
        return self.reactorWrapper.getRemoteObject(self.remoteReference, *args,
            **kwargs)
        

class RemoteControlWrapper(RemoteObjectWrapper):
    
    def __init__(self, reactorWrapper, host = 'localhost', port = LISTEN_PORT):
        
        call = GetRootObject(reactorWrapper.reactor, host = host, 
            port = port)
            
        reactorWrapper.reactor.callFromThread(call)
        call.wait(reactorWrapper.timeout)
        
        if isinstance(call.returncode, Failure):
            raiseRemoteException(call.returncode)
        elif not isinstance(call.returncode, pb.RemoteReference):
            raise RemoteException("Can't get remoteControl " + str(call.returncode))
            
        RemoteObjectWrapper.__init__(self, reactorWrapper, call.returncode)
        
    def uploadFile(self, fromFile, toFile = None, remoteFile = None):
        if not toFile:
            toFile = fromFile
            
        if not remoteFile:
            dir = os.path.dirname(toFile)
            if dir:
                self.call('makedirs', dir)
            remoteFile = self.getRemoteObject('open', toFile, 'wb+')
            
        localFile = open(fromFile, 'r')
        
        chunk = localFile.read(CHUNK_SIZE)
        while chunk:
            remoteFile.call('write', chunk)
            chunk = localFile.read(CHUNK_SIZE)
            
        localFile.close()
        remoteFile.call('close')
        

    def downloadFile(self, fromFile = None, toFile = None, remoteFile = None):
        if not toFile and not fromFile:
            raise AttributeError('either source or target must be specified')
            
        if not toFile and fromFile:
            toFile = fromFile
        elif not fromFile and toFile:
            fromFile = toFile
            
        if not remoteFile:
            dir = os.path.dirname(toFile)
            if dir:
                try:
                    os.makedirs(dir)
                except OSError:
                    pass
            remoteFile = self.getRemoteObject('open', fromFile, 'r')
            
        localFile = open(toFile, 'wb+')
        
        chunk = remoteFile.call('read', CHUNK_SIZE)
        while chunk:
            localFile.write(chunk)
            chunk = remoteFile.call('read', CHUNK_SIZE)
            
        localFile.close()
        remoteFile.call('close')
        

if __name__ == '__main__':
    (dirPath, scriptName) = os.path.split(sys.argv[0])
    if dirPath:
        os.chdir(dirPath)
        sys.argv[0] = './' + scriptName
    
    reactor.listenTCP(LISTEN_PORT, pb.PBServerFactory(RemoteControl()))
    reactor.run()
