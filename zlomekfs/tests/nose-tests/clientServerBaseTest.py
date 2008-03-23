# this should go first
from nose.twistedtools import threaded_reactor, stop_reactor

import tempfile
import re
import os
import logging
import shutil

from subprocess import Popen, PIPE, STDOUT
from os.path import walk
from insecticide import zfsConfig
from insecticide.snapshot import SnapshotDescription
from insecticide.timeoutPlugin import timed
from insecticide.graph import GraphBuilder

from zfs import ZfsStressTest, ZfsProxy, ZfsRuntimeException, abortDeadlock
from remoteZfs import RemoteControlWrapper, ReactorWrapper, RemoteException
from testFSOp import TestFSOp
from nose.tools import TimeExpired

rpm_list = ['zlomekfs', 'syplog', 'pysyplog', 'zfsd-status', 'insecticide']
local_files = ['zfs.py', 'remoteZfs.py', 'testFSOps.py, testFSOp.py', \
    'testStressFSOp.py']
# + all local files

log = logging.getLogger('nose.tests.clientServerBaseTest')

class TestClientServer(ZfsStressTest, TestFSOp):
    reactorWrapper = None
    remoteControlWrapper = None 
    remoteZfs = None
    
    definitionType = GraphBuilder.USE_FLAT
    
    startingPoint = "testGenerateNewName"
    
    
    @classmethod
    def uploadFiles(cls, files):
        #upload files
        for fileName in files:
            if os.path.isfile(fileName):
                cls.remoteControlWrapper.uploadFile(fromFile = fileName)
    
    @classmethod
    def syncRpms(cls):
        tempDir = tempfile.mkdtemp()
        rpms = []
        regex = '('
        
        # to force existence of /dev/fuse 
        modprobe = Popen(args = ('modprobe', 'fuse'), stdout = PIPE,
            stderr = STDOUT)
        modprobe.wait()
        if modprobe.returncode != 0:
            raise Exception('modprobe failed: %d(%s)' \
                % (modprobe.returncode, str(modprobe.stdout.readlines())))        
                
        # rebuild packages
        for rpmName in rpm_list:
            regex += '(' + rpmName + ')|'
            log.debug( 'build ' + rpmName + ' in ' + tempDir)
            packager = Popen(args=('rpmrebuild', '-b', '-d', tempDir, rpmName),
                stdout = PIPE, stderr = STDOUT)#, shell = True)
            packager.wait()
            if packager.returncode != 0:
                raise Exception('packager failed: %d(%s)' \
                    % (packager.returncode, str(packager.stdout.readlines())))
            
        regex = regex[:len(regex) - 1] + ').*\.rpm'
        match = re.compile(regex)
        
        def reportFile(rpmsSpec, dir, files):
            match = rpmsSpec[1]
            rpms = rpmsSpec[0]
            for file in files:
                if match.match(file):
                    rpms.append(os.path.join(dir, file))
                    
        walk(tempDir, reportFile, (rpms, match))
        
        cls.uploadFiles(rpms)
        
        cmd = ['rpm', '-Uvh', '--force']
        cmd.extend(rpms)
        
        update = cls.remoteControlWrapper.call('system', cmdLine = cmd)
        if update != 0:
            raise OSError('rpm update failed: %d' % update)
        
        shutil.rmtree(tempDir, True)
        # TODO: delete remote rpms too
        
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
        
        config = getattr(cls, zfsConfig.ZfsConfig.configAttrName)
        cls.config = config
        remoteTar = config.get("remoteZfs", "zfsMetaTar")
        local_files.append(remoteTar)
        
        cls.uploadFiles(local_files)
        
        # upload rpms
        #cls.syncRpms()
        
        # restart
        try:
            cls.reactorWrapper.setTimeout(2)
            restart = cls.remoteControlWrapper.call('restart')
            raise RemoteException("Restart returned (not expected): " + str(restart))
        except TimeExpired:
            pass
        finally:
            cls.reactorWrapper.setTimeout(20)
            
        cls.connect()
        
        # configure
        cls.setupRemoteZfs()
        cls.setupLocalZfs()
        
        #start
        cls.remoteZfs.call('start')
        cls.localZfs.runZfs()
        
    @classmethod
    def teardownClass(cls):
        #shutdown remote zfs
        
        cls.localZfs.stopZfs()
        cls.localZfs.cleanup()
        
        cls.remoteZfs.call('stop')
        cls.remoteZfs.call('cleanup')
        
        stop_reactor()
        cls.reactorWrapper = None
        cls.remoteControlWrapper = None
        cls.remoteZfs = None
        
    def snapshot(self, snapshot):
        self.localZfs.snapshot(snapshot)
        
        remoteFile = self.remoteZfs.getRemoteObject('snapshot')
        (handle, tempName) = tempfile.mkstemp()
        self.remoteControlWrapper.downloadFile(toFile = tempName, remoteFile = remoteFile)
        
        snapshot.addFile('remoteZfsSnapshot', tempName, 
            type = SnapshotDescription.TYPE_TAR_FILE)
        
        os.unlink(tempName)
        remoteFile.call('delete')
        
    def setup(self):
        self.raiseExceptionIfDied()
        
    def teardown(self):
        self.raiseExceptionIfDied()
        
    def raiseExceptionIfDied(self):
        if self.localZfs.hasDied() or \
            self.remoteZfs.call('hasDied'):
            log.debug("zfs has died")
            
            self.localZfs.stopZfs()
            self.remoteZfs.call('stop')
            raise ZfsRuntimeException("Zfsd died upon test execution")
            
    @timed(10, abortDeadlock)
    def testRemoteWriteLocalRead(self):
        remoteFile = self.remoteControlWrapper.getRemoteObject(
            'open', self.remoteFileName, 'w')
            
        data = "Kdyz se dobre hospodari, tak se dobre dari."
        remoteFile.call('write', data)
        remoteFile.call('flush')
        remoteFile.call('close')
        
        localFile = open(self.localFileName, 'r')
        readData = localFile.read()
        localFile.close()
        
        log.debug('remoteFile:' + self.remoteFileName)
        log.debug('localFile:' + self.localFileName)
        log.debug( "data is :" + str(data))
        log.debug("readData is:" + str(readData))
        assert data == readData
        
    @timed(10, abortDeadlock)
    def tesLocalWriteRemoteRead(self):
        data = "Kdyz se dobre hospodari, tak se dobre dari."
        localFile = open(self.localFileName, 'w')
        localFile.write(data)
        localFile.close()
        
        remoteFile = self.remoteControlWrapper.getRemoteObject(
            'open', self.remoteFileName, 'r')
        readData = remoteFile.call('read')
        remoteFile.call('close')
        
        log.debug('remoteFile:' + self.remoteFileName)
        log.debug('localFile:' + self.localFileName)
        log.debug( "data is :" + str(data))
        log.debug("readData is:" + str(readData))
        
        assert data == readData
        
    def testGenerateNewName(self):
        fileName = self.generateRandomFileName()
        
        self.localFileName = os.path.join(self.localZfsDir, fileName)
        self.remoteFileName = os.path.join(self.remoteZfsDir, fileName)
        
