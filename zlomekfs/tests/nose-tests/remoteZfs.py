#! /bin/env python

from twisted.spread import pb
from twisted.internet import reactor
import sys
import os
from insecticide.snapshot import SnapshotDescription
import tempfile
from subprocess import Popen
from threading import Condition
from zfs import ZfsProxy
from nose.tools import TimeExpired


LISTEN_PORT = 8007

class RemoteFile(pb.Referenceable):
    def __init__(self, fileName, mode):
        self.fh=open(fileName, mode)
        
    def remote_write(self, data):
        return self.fh.write(data)
        
    def remote_read(self, size):
        return self.fh.read(size)
        
    def remote_getSize(self):
        pos = self.fh.tell()
        self.fh.seek(0,os.SEEK_END)
        size = self.fh.tell()
        self.fh.seek(pos,os.SEEK_SET)
        return size
        
    def remote_close(self):
        return self.fh.close()
        
    def remote_seek(self, offset):
        return self.fh.seek(offset, os.SEEK_SET)
        
    def remote_tell(self):
        return self.fh.tell()
        
    def remote_getName(self):
        return self.fh.name
        
    def remote_delete(self):
        os.unlink(self.fh.name)

class RemoteZfs(pb.Referenceable):
    def __init__(self, *arg, **kwargs):
        self.zfs = ZfsProxy(*arg, **kwargs)
        
    def remote_start(self):
        return self.zfs.runZfs()
    
    def remote_stop(self):
        return self.zfs.stopZfs()
        
    def remote_cleanup(self):
        return self.zfs.cleanup()
    
    def remote_running(self):
        return self.zfs.running()
    
    def remote_hasDied(self):
        return self.zfs.hasDied()
        
    def remote_snapshot(self):
        toDir = tempfile.mkdtemp(prefix="noseSnapshot")
        snapshot = SnapshotDescription(toDir)
        self.zfs.snapshot(snapshot)
        (handle,fileName) = tempfile.mkstemp()
        snapshot.pack(fileName)
        snapshot.delete()
        return RemoteFile(fileName, 'r')
        

class RemoteControl(pb.Root):
    def remote_system(self, cmdLine):
        print 'executing ' + str(cmdLine)
        proc = Popen(args = cmdLine)
        proc.wait()
        return proc.returncode
        
    def remote_open(self, fileName, mode):
        return RemoteFile(fileName, mode)
        
    def remote_makedirs(self, dirName):
        try:
            return os.makedirs(dirName)
        except OSError:
            pass
        
    def remote_delete(self, fileName):
        return os.unlink(fileName)
        
    def remote_restart(self):
        print 'executing ' + str(sys.argv)
        reactor.stop()
        os.execv(sys.argv[0],sys.argv)
        
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
        
        
    @classmethod
    def callDirect(cls, reactor, timeout = None, *arg, **kwargs):
        call = cls(*arg, **kwargs)
        reactor.callFromThread(call)
        #call = cls(*arg, **kwargs)
        call.wait(timeout = timeout)
        
        return call.returncode
        
    def wait(self, timeout = None):
        self.cond.acquire()
        if not self.returned:
            self.cond.wait(timeout)
            
        self.cond.release()
        if not self.returned:
            raise TimeExpired('timeout')


def printit(obj):
    print obj
    
class GetRemoteControl(SimpleRemoteCall):
    def __init__(self, reactor, host = 'localhost', port = LISTEN_PORT):
        self.cond = Condition()
        
        factory = pb.PBClientFactory()
        reactor.connectTCP(host, port, factory)
        
        deref = factory.getRootObject()
        deref.addCallbacks(getattr(self,'successHandler'), getattr(self, 'errorHandler'))
        
    @classmethod
    def callDirect(cls, reactor, timeout = None, *arg, **kwargs):
        call = cls(reactor, *arg, **kwargs)
        reactor.callFromThread(call)
        #call = cls(*arg, **kwargs)
        call.wait(timeout = timeout)
        
        return call.returncode
        
    
CHUNK_SIZE = 4096

def uploadFile(reactor, remoteControl, fromFile, toFile = None, remoteFile = None):
    print 'uploading ' + fromFile
    if not toFile:
        toFile = fromFile
        
    if not remoteFile:
        dir = os.path.dirname(toFile)
        if dir:
            SimpleRemoteCall.callDirect(reactor, 10, remoteControl, 'makedirs', dir)
        remoteFile = SimpleRemoteCall.callDirect(reactor, 10, remoteControl, 'open', toFile, 'wb+')
        if isinstance(remoteFile, pb.CopiedFailure):
            raise Exception("can't open remote file: " + str(remoteFile))
        
    localFile = open(fromFile, 'r')
    
    chunk = localFile.read(CHUNK_SIZE)
    while chunk:
        SimpleRemoteCall.callDirect(reactor, 20, remoteFile, 'write', chunk)
        chunk = localFile.read(CHUNK_SIZE)
        
    localFile.close()
    SimpleRemoteCall.callDirect(reactor, 10, remoteFile, 'close')
    

def downloadFile(reactor, remoteControl, fromFile = None, toFile = None, remoteFile = None):
    if not toFile and not fromFile:
        raise Exception('either source or target must be specified')
        
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
        remoteFile = SimpleRemoteCall.callDirect(reactor, 10, remoteControl, 'open', fromFile, 'r')
        if isinstance(remoteFile, pb.CopiedFailure):
            raise Exception("can't open remote file: " + str(remoteFile))
        
    localFile = open(toFile, 'w+b')
    
    chunk = SimpleRemoteCall.callDirect(reactor, 10, remoteFile, 'read', CHUNK_SIZE)
    while chunk:
        localFile.write(chunk)
        chunk = SimpleRemoteCall.callDirect(reactor, 10, remoteFile, 'read', CHUNK_SIZE)
        
    localFile.close()
    SimpleRemoteCall.callDirect(reactor, 10, remoteFile, 'close')

if __name__ == '__main__':
    reactor.listenTCP(LISTEN_PORT, pb.PBServerFactory(RemoteControl()))
    reactor.run()
