from insecticide.snapshot import SnapshotDescription
from subprocess import Popen, PIPE
import subprocess
import signal
import pysyplog
import tarfile
import tempfile
import os
import shutil
import zfsd_status  
import time

from insecticide import graph
from insecticide import zfsConfig

class ZfsProxy(object):
    usedCompression = ""
    running = False
    zfsRoot = "/mnt/zfs"
    tempDir = "/tmp/zfsTestProxyTemp"
    zfsCacheDir = "cache"
    logFileName = "zfsd.log"
    config = "etc" + os.sep + "config"
    
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
        if pysyplog.ping_syplog_dbus(None) != pysyplog.NOERR:
          raise Exception ("Syplog offline")
        
    def disconnectControl(self):
        pass
        
    def changeZfsLogLevel(self, loglevel):
        if not self.running:
            raise Exception("zfs not running") #TODO: accurate exception
        
        pysyplog.set_level_udp(loglevel, None, 0)
        
    def runZfs(self):
        self.unpackData()
        self.installModules()
        loglevel = pysyplog.LOG_LOOPS
        if self.logger:
            loglevel = pysyplog.get_log_level(logger)
        self.zfs = Popen(args=('zfsd',
                                '-d',
                                "--" + str(pysyplog.PARAM_MEDIUM_TYPE_LONG) + "=" + str(pysyplog.FILE_MEDIUM_NAME),
                                "--" + str(pysyplog.PARAM_MEDIUM_FMT_LONG) + "=" + str(pysyplog.USER_READABLE_FORMATTER_NAME),
                                "--" + str(pysyplog.PARAM_MEDIUM_FN_LONG) + "=" + self.tempDir + os.sep + ZfsProxy.logFileName,
                                '-o', 'loglevel=' + str(loglevel) +
                                ',config=' + self.tempDir + os.sep + self.config, 
                                self.zfsRoot),
                                cwd = self.tempDir,
                                stdout = PIPE, stderr = PIPE, universal_newlines=True) # FIXME: core dump reporting
        for i in [0.2, 0.5, 1, 3]:
            time.sleep(i)
            if zfsd_status.ping_zfsd() == zfsd_status.ZFSD_STATE_RUNNING:
              break
        if zfsd_status.ping_zfsd() != zfsd_status.ZFSD_STATE_RUNNING:
            raise Exception("Zfsd doesn't start")
        self.running = True
        self.connectControl()
        
    def stopZfs(self):
        #TODO: check status
        self.disconnectControl()
        for i in [0.1, 0.5, 1]:
            os.kill(self.zfs.pid, signal.SIGTERM)
            time.sleep(i)
            if self.zfs.poll():
                break
        
        if self.zfs.poll () is None:
          os.kill(self.zfs.pid, signal.SIGKILL)

        Popen(args=('umount', '-f', self.zfsRoot))
        self.running = False
        self.removeModules()
        self.cleanup()
        
    def snapshot(self, snapshot):
        #snapshot cache
        try:
            snapshot.addDir(name = 'zfsCache',  
                        sourceDirName = self.tempDir + os.sep + self.zfsCacheDir,
                        type = SnapshotDescription.TYPE_ZFS_CACHE)
        except OSError: #can arise before first run
            pass
        #snapshot log
        #NOTE: this could fail when zfsd doesn't start yet
        try:
            snapshot.addFile(name = 'zfsd.log',
                         sourceFileName = self.tempDir + os.sep + self.logFileName,
                         type = SnapshotDescription.TYPE_ZFS_LOG)
        except IOError:
            pass

        if self.running:
        #snapshot zfs process
            if not self.zfs.returncode:
                gdb = subprocess.Popen(args=('gdb', '-p',  str(self.zfs.pid), ), stdin = PIPE, 
                                        stdout=PIPE, stderr=PIPE, universal_newlines=True)
                gdb.stdin.write('gcore ' + self.tempDir + os.sep + 'zfsd.core.' + str(self.zfs.pid) + '\n')
                gdb.stdin.write('quit' + '\n')
                gdb.wait()
                if gdb.returncode != 0:
                    raise Exception(gdb.stderr.readlines()) #FIXME: accurate exception
		try:
                    snapshot.addFile(name = 'zfs.core',  sourceFileName = self.tempDir + os.sep + 'zfsd.core.' + str(self.zfs.pid), 
                                 type = SnapshotDescription.TYPE_ZFS_GCORE)
                except IOError:
                    pass #FIXME: this should not happen
                         
            #set as unresumable
            snapshot.addEntry('canResume', 
                          (SnapshotDescription.TYPE_BOOL, False))
        if hasattr(self,"zfs"):
                snapshot.addObject("zfsStderr",self.zfs.stderr)#TODO: don't block waiting
                snapshot.addObject("zfsStdout",self.zfs.stdout)

        
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
	snapshot.addObject("test",self)
        
        # snapshot config
        config = getattr(self, zfsConfig.ZfsConfig.configAttrName,  None)
        if config:
            snapshot.addConfig(config)
        #TODO: snapshot script (it may change)
        
    def resume(self,  snapshot):
        # resume config
        try:
            config = snapshot.getConfig()
        except KeyError:
            pass
        setattr(self, zfsConfig.ZfsConfig.configAttrName,  config)
        
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
