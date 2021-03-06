# this should go first
from nose.twistedtools import threaded_reactor

import tempfile
import os
import logging
import sys
import signal

from insecticide import zfsConfig
from insecticide.snapshot import SnapshotDescription
from insecticide.timeoutPlugin import timed
from insecticide.graph import GraphBuilder

from zfs import abortDeadlock as abortLocalDeadlock
from zfs import ZfsStressTest, ZfsProxy, ZfsRuntimeException, forceDeleteFile
from remoteZfs import RemoteControlWrapper, ReactorWrapper, RemoteException
from testFSOp import TestFSOp

rpm_list = ['zlomekfs', 'syplog', 'pysyplog', 'zfsd-status', 'insecticide']
local_files = ['zfs.py', 'remoteZfs.py', 'testFSOps.py, testFSOp.py', \
    'testStressFSOp.py']
# + all local files

log = logging.getLogger('nose.tests.clientServerBaseTest')

def abortDeadlock():
    abortLocalDeadlock()
    
    cls = TestClientServer
    if cls.reactorWrapper:
        cls.reactorWrapper.setTimeout(cls.longTimeout)
        
        if cls.remoteZfs:
            cls.remoteZfs.call('signalAll', signal.SIGABRT)
            
        cls.reactorWrapper.setTimeout(cls.defaultTimeout)
        
class TestGlobalState(object):
    """ Global state holder for TestClientServer
        Holds file handles (to allow close from within
        class context.
        """
    def __init__(self):
        self.localFile = None
        self.remoteFile = None
        
            
    def clean(self):
        if self.localFile:
            forceDeleteFile(self.safeFile)
            self.safeFile = None
            
        if self.remoteFile:
            try:
                self.remoteFile.call('close')
            except KeyboardInterrupt:
                raise
            except Exception:
                pass
            try:
                self.remoteFile.call('delete')
            except KeyboardInterrupt:
                raise
            except Exception:
                pass
            self.remoteFile = None

class TestClientServer(ZfsStressTest, TestFSOp):
    reactorWrapper = None
    remoteControlWrapper = None 
    remoteZfs = None
    defaultTimeout = 60
    longTimeout = 240
    definitionType = GraphBuilder.USE_FLAT
    globalState = None
    
    startingPoint = "testGenerateNewName"
    
    @classmethod
    def uploadFiles(cls, files):
        #upload files
        for fileName in files:
            if os.path.isfile(fileName):
                cls.remoteControlWrapper.uploadFile(fromFile = fileName)
        
    @classmethod
    def connect(cls):
        config = getattr(cls, zfsConfig.ZfsConfig.configAttrName)
        targetHost = config.get("remoteZfs", "hostname")
        
        cls.remoteControlWrapper = RemoteControlWrapper(cls.reactorWrapper,
            host =  targetHost)
            
        
    @classmethod
    def setupRemoteZfs(cls):
        config = getattr(cls, zfsConfig.ZfsConfig.configAttrName)
        
        remoteTar = config.get("remoteZfs", "zfsMetaTar")
        remoteZfsConfig = config.get("remoteZfs", "zfsConfigFile")
        
        cls.remoteZfs = cls.remoteControlWrapper.getRemoteObject('getZfs', 
            metaTar = remoteTar, config = remoteZfsConfig)
            
        remoteRoot = cls.remoteZfs.call('getMountPoint')
        cls.remoteZfsDir = os.path.join(remoteRoot, 
            config.get("remoteZfs", "zfsVolumeDir"))
        
        
    @classmethod
    def setupLocalZfs(cls):
        config = getattr(cls, zfsConfig.ZfsConfig.configAttrName)
        
        localRoot = tempfile.mkdtemp(prefix = 'zfsMountPoint')
        localTar = config.get("localZfs", "zfsMetaTar")
        localZfsConfig = config.get("localZfs", "zfsConfigFile")
        
        cls.localZfs = ZfsProxy(metaTar = localTar, zfsRoot = localRoot,
            config = localZfsConfig)
            
        cls.localZfsDir = os.path.join(localRoot, 
            config.get("localZfs", "zfsVolumeDir"))
        
    @classmethod
    def setupClass(cls):
        
        # start twisted and get root handle
        reactor, reactorThread = threaded_reactor()
        cls.reactorWrapper = ReactorWrapper(reactor, reactorThread)
        
        cls.connect()
        
        # restart remote zfs - there could be update available
        try:
            cls.reactorWrapper.setTimeout(5)
            restart = cls.remoteControlWrapper.call('restart')
            raise RemoteException("Restart returned (not expected): " + str(restart))
        except KeyboardInterrupt:
            raise
        except Exception:
            pass
        finally:
            cls.reactorWrapper.setTimeout(cls.defaultTimeout)
            
        cls.connect()
        
        # configure
        cls.setupRemoteZfs()
        cls.setupLocalZfs()
        
        #start
        cls.remoteZfs.call('start')
        cls.localZfs.runZfs()
        
        cls.globalState = TestGlobalState()
        
    @classmethod
    def teardownClass(cls):
        #shutdown remote zfs
        cls.globalState.clean()
        
        cls.localZfs.stopZfs()
        cls.localZfs.cleanup()
        
        cls.remoteZfs.call('stop')
        cls.remoteZfs.call('cleanup')
        
        cls.reactorWrapper = None
        cls.remoteControlWrapper = None
        cls.remoteZfs = None
        
    def snapshot(self, snapshot):
        self.localZfs.snapshot(snapshot)
        
        self.reactorWrapper.setTimeout(self.longTimeout)
        remoteFile = self.remoteZfs.getRemoteObject('snapshot')
        (handle, tempName) = tempfile.mkstemp()
        self.remoteControlWrapper.downloadFile(toFile = tempName, remoteFile = remoteFile)
        self.reactorWrapper.setTimeout(self.defaultTimeout)
        
        snapshot.addFile('remoteZfsSnapshot', tempName, 
            entryType = SnapshotDescription.TYPE_TAR_FILE)
        
        os.unlink(tempName)
        remoteFile.call('delete')
        
    def setup(self):
        self.raiseExceptionIfDied()
        
    def teardown(self):
        if sys.exc_info() is (None, None, None):
            self.raiseExceptionIfDied()
        
    def raiseExceptionIfDied(self):
        if self.localZfs.hasDied() or \
            self.remoteZfs.call('hasDied'):
            log.debug("zfs has died")
            
            self.localZfs.stopZfs()
            self.remoteZfs.call('stop')
            raise ZfsRuntimeException("Zfsd died upon test execution")
    raiseExceptionIfDied.metaTest = False
            
    @timed(120, abortDeadlock)
    def testRemoteWriteLocalRead(self):
        self.globalState.remoteFile = self.remoteControlWrapper.getRemoteObject(
            'open', self.remoteFileName, 'w')
            
        data = "Kdyz se dobre hospodari, tak se dobre dari."
        self.globalState.remoteFile.call('write', data)
        self.globalState.remoteFile.call('flush')
        self.globalState.remoteFile.call('close')
        self.globalState.remoteFile = None
        
        self.globalState.localFile = open(self.localFileName, 'r')
        readData = self.globalState.localFile.read()
        self.globalState.localFile.close()
        self.globalState.localFile = None
        
        log.debug('remoteFile:' + self.remoteFileName)
        log.debug('localFile:' + self.localFileName)
        log.debug( "data is :" + str(data))
        log.debug("readData is:" + str(readData))
        
        self.raiseExceptionIfDied()
        
        assert data == readData
        
    @timed(120, abortDeadlock)
    def tesLocalWriteRemoteRead(self):
        data = "Kdyz se dobre hospodari, tak se dobre dari."
        self.globalState.localFile = open(self.localFileName, 'w')
        self.globalState.localFile.write(data)
        self.globalState.localFile.close()
        self.globalState.localFile = None
        
        self.globalState.remoteFile = self.remoteControlWrapper.getRemoteObject(
            'open', self.remoteFileName, 'r')
        readData = self.globalState.remoteFile.call('read')
        self.globalState.remoteFile.call('close')
        self.globalState.remoteFile = None
        
        log.debug('remoteFile:' + self.remoteFileName)
        log.debug('localFile:' + self.localFileName)
        log.debug( "data is :" + str(data))
        log.debug("readData is:" + str(readData))
        
        self.raiseExceptionIfDied()
        
        assert data == readData
        
    def testGenerateNewName(self):
        fileName = self.generateRandomFileName()
        
        self.localFileName = os.path.join(self.localZfsDir, fileName)
        self.remoteFileName = os.path.join(self.remoteZfsDir, fileName)
        
