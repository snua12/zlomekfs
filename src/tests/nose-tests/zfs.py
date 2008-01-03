from snapshot import SnapshotDescription
from subprocess import Popen, PIPE
import subprocess
import signal
import pysyplog
import tarfile
import tempfile
import os
import shutil

import graph
import zfsConfig

class ZfsProxy(object):
    usedCompression = ""
    running = False
    zfsRoot = "/mnt/zfs"
    tempDir = "/tmp/zfsTestProxyTemp"
    zfsCache = tempDir + os.sep + "cache"
    logFileName = "zfsd.log"
    config = tempDir + os.sep + "etc" + os.sep + "config"
    
    running = False
    def __init__(self, metaTar, zfsRoot = None,  tempDir = None,   logger = None):
        if zfsRoot:
            self.zfsRoot = zfsRoot
           
        self.logger = logger
        
        self.metaTar = metaTar
        
        if tempDir:
          self.tempDir = tempDir
        else:
          self.tempDir =  tempfile.mkdtemp(prefix = "zfsTestTemp")
          
    def installModules(self):
        modprobe = subprocess.Popen(args=('modprobe', 'fuse'), stdout=PIPE, 
                                stderr=PIPE, universal_newlines=True)
        modprobe.wait()
        if modprobe.returncode != 0:
            raise Exception(modprobe.stderr.readlines()) #FIXME: accurate exception
        
    def removeModules(self):
        Popen(args=('rmmod', '-f', 'fuse'))
        
    def unpackData(self):
        tarFile = tarfile.open(name = self.metaTar, 
                               mode = 'r' + self.usedCompression)
        tarFile.extractall(self.tempDir)
        tarFile.close()
        
    def cleanup(self): #TODO: cleanup without tempDir removal
        shutil.rmtree(self.tempDir, True)
        os.mkdir(self.tempDir)
        
    def connectControl(self):
        pass
        
    def disconnectControl(self):
        pass
        
    def changeZfsLogLevel(self, loglevel):
        if not self.running:
            raise Exception("zfs not running") #TODO: accurate exception
        
        pysyplog.set_level_udp(loglevel, None, 0)
        
    def runZfs(self):
        self.unpackData()
        self.installModules()
        loglevel = pysyplog.DEFAULT_LOG_LEVEL
        if self.logger:
            loglevel = pysyplog.get_log_level(logger)
        self.zfs = Popen(args=('zfsd',
                                '-d',
                                "--" + str(pysyplog.PARAM_MEDIUM_TYPE_LONG) + "=" + str(pysyplog.FILE_MEDIUM_NAME),
                                "--" + str(pysyplog.PARAM_MEDIUM_FMT_LONG) + "=" + str(pysyplog.USER_READABLE_FORMATER_NAME),
                                "--" + str(pysyplog.PARAM_MEDIUM_FN_LONG) + "=" + self.tempDir + os.sep + ZfsProxy.logFileName,
                                '-o', 'loglevel=' + str(loglevel) +
                                ',config=' + self.config, 
                                self.zfsRoot),
                                cwd = self.tempDir,
                                stdout = PIPE, stderr = PIPE, universal_newlines=True) # FIXME: core dump reporting
        
        self.running = True
        self.connectControl()
        
    def stopZfs(self):
        #TODO: check status
        self.disconnectControl()
        os.kill(self.zfs.pid, signal.SIGTERM)
        Popen(args=('umount', '-f', self.zfsRoot))
        self.running = False
        self.removeModules()
        self.cleanup()
        
    def snapshot(self, snapshot):
        #snapshot cache
        snapshot.addDir(name = 'zfsCache',  
                        sourceDirName = self.zfsCache,
                        type = SnapshotDescription.TYPE_ZFS_CACHE)
        #snapshot log
        snapshot.addFile(name = 'zfsd.log',
                         sourceFileName = self.tempDir + os.sep + logFileName,
                         type = SnapshotDescription.TYPE_ZFS_LOG)
        #FIXME : snapshot zfs process
        (commandFile,  commandFileName) = shutil.mkstemp(prefix = 'gdbCommand')
        commandFile.write('gcore ' + self.tempDir + os.sep + 'zfsd.core.' + str(self.zfs.pid) + '\n')
        commandFile.write('quit')
        commandFile.close()
        gdb = subprocess.Popen(args=('gdb', '-p',  str(self.zfs.pid),  '-x',  commandFileName ), stdout=PIPE, 
                                stderr=PIPE, universal_newlines=True)
        gdb.wait()
        if gdb.returncode != 0:
            raise Exception(gdb.stderr.readlines()) #FIXME: accurate exception
        snapshot.addFile(name = 'zfs.core',  sourceFileName = self.tempDir + os.sep + 'zfsd.core' + str(self.zfs.pid), 
                         type = SnapshotDescription.TYPE_ZFS_GCORE)
                         
        if self.running:
            #set as unresumable
            snapshot.addEntry('canResume', 
                          (SnapshotDescription.TYPE_BOOL, False))
        
    def resume(self, snapshot):
        try:
            (type, canResume) = snapshot.getEntry("canResume")
            if type == SnapshotDescription.TYPE_BOOL and not canResume:
                raise Exception ("snapshot is not resumable")
        except KeyError:
            pass
        

class ZfsTest(object):

    @classmethod
    def setup_class(self):
        config = getattr(self, zfsConfig.ZfsConfig.configAttrName)
        self.zfsRoot = config.get("global", "zfsRoot")
        self.zfsCache = config.get("global", "zfsCache")
        self.zfsMetaTar = config.get("global", "zfsMetaTar")

        self.zfs = ZfsProxy(zfsRoot = self.zfsRoot,  metaTar = self.zfsMetaTar)
    
    def setup(self):
        self.zfs.runZfs()
        
    def teardown(self):
        #TODO: raise exception if zfs is down
        self.zfs.stopZfs()
        
    @classmethod
    def teardown_class(self):
        self.zfs = None
    
    def snapshot(self, snapshot):        
        # snapshot zfs
        self.zfs.snapshot(snapshot)
        
        snapshot.addObject("zfsProxy", self.zfs)
        
        # snapshot config
        config = getattr(self, zfsConfig.zfsConfig.configAttrName,  None)
        if config:
            snapshot.addConfig(config)
        #TODO: snapshot script (it may change)
        
    def resume(self,  snapshot):
        # resume config
        try:
            config = snapshot.getConfig()
        except KeyError:
            pass
        setattr(self, zfsConfig.zfsConfig.configAttrName,  config)
        
        self.zfs = snapshot.getObject("zfsProxy")
        
        # resume zfs
        self.zfs.resume(snapshot)
        

class ZfsStressTest(ZfsTest):
    metaTest = True
    definitionType = graph.GraphBuilder.USE_FLAT
    
    def __init__(self):
        ZfsTest.__init__(self)
    
    # we don't want to reset state after every test
    def setup(self):
        pass
    def teardown(self):
        pass
        
    # do the before test setup only once before all tests
    def setup_class(self):
        ZfsTest.setup_class(self)
        ZfsTest.setup(self)
        
    # do the after test cleanup only once after all tests
    def teardown_class(self):
        ZfsTest.teardown(self)
        ZfsTest.teardown_class(self)
