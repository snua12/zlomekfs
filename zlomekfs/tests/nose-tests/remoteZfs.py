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
    def __init__(self, args):
        self.zfs = ZfsProxy(args)
        
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
    def remote_system(self, args):
         proc = Popen(args = args)
         proc.wait()
         return proc.returncode
         
    def remote_open(self, fileName, mode):
        return RemoteFile(fileName, mode)
        
    def remote_makedirs(self, dirName):
        return os.makedirs(dirName)
        
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
        
    def __init__(self, remoteReference, *arg, **kwargs):
        deref = remoteReference.callRemote(*arg, **kwargs)
        self.cond = Condition()
        deref.addCallbacks(self.successHandler, self.errorHandler)
        
    @classmethod
    def callDirect(cls, *arg, **kwargs):
        call = cls(*arg, **kwargs)
        call.wait()
        
        return call.returncode
        
    def wait(self, timeout = None):
        self.cond.acquire()
        if not self.returned:
            self.cond.wait(timeout)
            
        self.cond.release()
        if not self.returned:
            raise Exception('timeout')
    
class GetRemoteControl(SimpleRemoteCall):
    def __init__(self, reactor, host = 'localhost', port = LISTEN_PORT):
        factory = pb.PBClientFactory()
        reactor.connectTCP(host, port, factory)
        deref = factory.getRootObject()
        deref.addCallbacks(self.successHandler, self.errorHandler)
    
CHUNK_SIZE = 4096

def uploadFile(remoteControl, fromFile, toFile = None, remoteFile = None):
    if not toFile:
        toFile = fromFile
        
    if not remoteFile:
        SimpleRemoteCall.callDirect(remoteControl, 'makedirs', os.path.basename(toFile))
        remoteFile = SimpleRemoteCall.callDirect(remoteControl, open, toFile, 'w')
        
    localFile = open(fromFile, 'r')
    
    chunk = localFile.read(CHUNK_SIZE)
    while chunk:
        SimpleRemoteCall.callDirect(remoteFile, 'write', chunk)
        chunk = localFile.read(CHUNK_SIZE)
        
    localFile.close()
    SimpleRemoteCall.callDirect(remoteFile, 'close')
    

def downloadFile(remoteControl, fromFile = None, toFile = None, remoteFile = None):
    if not toFile and not fromFile:
        raise Exception('either source or target must be specified')
        
    if not toFile and fromFile:
        toFile = fromFile
    elif not fromFile and toFile:
        fromFile = toFile
        
    if not remoteFile:
        os.makedirs(os.path.basename(toFile))
        remoteFile = SimpleRemoteCall.callDirect(remoteControl, open, fromFile, 'r')
        
    localFile = open(toFile, 'w')
    
    chunk = SimpleRemoteCall.callDirect(remoteFile, 'read', CHUNK_SIZE)
    while chunk:
        localFile.write(chunk)
        chunk = SimpleRemoteCall.callDirect(remoteFile, 'read', CHUNK_SIZE)
        
    localFile.close()
    SimpleRemoteCall.callDirect(remoteFile, 'close')

if __name__ == '__main__':
    reactor.listenTCP(LISTEN_PORT, pb.PBServerFactory(RemoteControl()))
    reactor.run()
