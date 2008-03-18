# this should go first
from nose.twistedtools import threaded_reactor, stop_reactor

import tempfile
import re
import os
import logging
import shutil

from subprocess import Popen, PIPE, STDOUT
from os.path import walk
from twisted.spread import pb
from insecticide import zfsConfig
from insecticide.snapshot import SnapshotDescription

from zfs import ZfsStressTest, ZfsProxy, ZfsRuntimeException
from remoteZfs import uploadFile, downloadFile
from remoteZfs import GetRemoteControl, SimpleRemoteCall
from testFSOp import TestFSOp

rpm_list = ['zlomekfs', 'syplog', 'pysyplog', 'zfsd-status', 'insecticide']
# + all local files

log = logging.getLogger('nose.tests.clientServerBaseTest')

class ClientServerBaseTest(ZfsStressTest, TestFSOp):
    reactor = None
    reactorThread = None
    remoteZfs = None
    
    
    @classmethod
    def getRpms(cls, tempDir = '/tmp/'):
        rpms = []
        regex = ''
        
        # to force existence of /dev/fuse 
        modprobe = Popen(args = ('modprobe', 'fuse'), STDOUT = PIPE,
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
            
        regex = regex[:len(regex) - 1] + '.*\.rpm'
        match = re.compile(regex)
        
        def reportFile(rpmsSpec, dir, files):
            match = rpmsSpec[1]
            rpms = rpmsSpec[0]
            for file in files:
                if match.match(file):
                    rpms.append(os.path.join(dir, file))
                    
        walk(tempDir, reportFile, (rpms, match))
        
        return rpms
        
    @classmethod
    def connect(cls):
        config = getattr(cls, zfsConfig.ZfsConfig.configAttrName)
        targetHost = config.get("remoteZfs", "hostname")
        
        cls.remoteControl = GetRemoteControl.callDirect(cls.reactor, targetHost)
        if not isinstance(cls.remoteControl, pb.RemoteReference):
            raise Exception ("can't get remoteControl")
        
        
    @classmethod
    def setupRemoteZfs(cls):
        config = getattr(cls, zfsConfig.ZfsConfig.configAttrName)
        remoteRoot = config.get("remoteZfs", "zfsRoot")
        remoteTar = config.get("remoteZfs", "zfsMetaTar")
        
        cls.remoteZfs = SimpleRemoteCall.callDirect(cls.remoteControl, 'getZfs',
            metaTar = remoteTar, zfsRoot = remoteRoot)
        if not isinstance(cls.remoteZfs, pb.RemoteReference):
            raise Exception('getZfs failed')
        
        
    @classmethod
    def setupLocalZfs(cls):
        config = getattr(cls, zfsConfig.ZfsConfig.configAttrName)
        localRoot = config.get("localZfs", "zfsRoot")
        localTar = config.get("localZfs", "zfsMetaTar")
        
        cls.localZfs = ZfsProxy(metaTar = localTar, zfsRoot = localRoot)
        
    @classmethod
    def setupClass(cls):
        
        # start twisted and get root handle
        cls.reactor, cls.reactorThread = threaded_reactor()
        cls.connect()
        
        #upload files
        localFiles = os.listdir('.')
        for fileName in localFiles:
            uploadFile(remoteControl = cls.remoteControl, fromFile = fileName)
        
        # upload rpms
        tempDir = tempfile.mkdtemp()
        rpms = cls.getRpms(tempDir)
        
        for rpm in rpms:
            uploadFile(remoteControl = cls.remoteControl, fromFile = rpm)
            
        update = SimpleRemoteCall.callDirect(cls.remoteControl,'system', ['rpm', '-Uvh'].extend(rpms))
        if update != 0:
            raise Exception('rpm update failed: %d' % update)
        
        shutil.rmtree(tempDir, True)
        
        # restart
        restart = SimpleRemoteCall(cls.remoteControl, 'restart')
        # TODO: bypass errors, etc
        
        cls.connect()
        
        # configure
        cls.startRemoteZfs()
        
        #start
        SimpleRemoteCall.callDirect(cls.remoteZfs, 'start')
        cls.localZfs.runZfs()
        
    @classmethod
    def teardownClass(cls):
        #shutdown remote zfs
        stop_reactor()
        cls.reactorThread = None
        
        cls.localZfs.stopZfs()
        cls.localZfs.clenaup()
        
        SimpleRemoteCall.callDirect(cls.remoteZfs, 'stop')
        SimpleRemoteCall.callDirect(cls.remoteZfs, 'cleanup')
        
    def snapshot(self, snapshot):
        self.localZfs.snapshot(snapshot)
        
        remoteFile = SimpleRemoteCall.callDirect(self.remoteZfs, 'snapshot')
        (handle, tempName) = tempfile.mkstemp()
        downloadFile(toFile = tempName, remoteFile = remoteFile)
        
        snapshot.addFile('remoteZfsSnapshot', tempName, 
            type = SnapshotDescription.TYPE_TAR_FILE)
        
        os.unlink(tempName)
        SimpleRemoteCall.callDirect(remoteFile, 'delete')
        
    def setup(self):
        self.raiseExceptionIfDied()
        
    def teardown(self):
        self.raiseExceptionIfDied()
        
    def raiseExceptionIfDied(self):
        if self.localZfs.hasDied() or \
            SimpleRemoteCall.callDirect(self.remoteZfs, 'hasDied'):
            log.debug("zfs has died")
            
            self.localZfs.stopZfs()
            SimpleRemoteCall.callDirect(self.remoteZfs, 'stop')
            raise ZfsRuntimeException("Zfsd died upon test execution")

