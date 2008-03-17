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

from zfs import ZfsStressTest
from remoteZfs import uploadFile, downloadFile, SimpleRemoteCall, LISTEN_PORT
from remoteZfs import GetRemoteControl
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
        self.remoteControl = GetRemoteControl.callDirect(self.reactor, host)
        if not isinstance(self.remoteControl, pb.RemoteReference):
            raise Exception ("can't get remoteControl")
        
    @classmethod
    def setupClass(cls):
        # setup local zfs
        super(testStressFSOp,cls).setupClass()
        
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
        # get and run zfs
        cls.remoteZfs = SimpleRemoteCall.callDirect(cls.remoteControl, 'getZfs')
        if not isinstance(cls.remoteZfs, pb.RemoteReference):
            raise Exception('getZfs failed')
        
        SimpleRemoteCall.callDirect(cls.remoteZfs, 'start')
        
    @classmethod
    def teardownClass(cls):
        #shutdown remote zfs
        stop_reactor()
        cls.reactorThread = None
        
        super(testStressFSOp,cls).teardownClass()
    
    
