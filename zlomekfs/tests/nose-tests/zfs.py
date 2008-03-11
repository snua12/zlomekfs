import signal
import pysyplog
import tarfile
import tempfile
import os
import shutil
import zfsd_status  
import time
import logging


from insecticide.snapshot import SnapshotDescription
from insecticide.util import allowCoreDumps, setCoreDumpSettings

from subprocess import Popen, PIPE, STDOUT
from traceback import format_exc
from insecticide import graph
from insecticide import zfsConfig

log = logging.getLogger ("nose.tests.zfs")


class ZfsRuntimeException(Exception):
    """ Exception raised upon zfs daemon failure """
    pass

class ZfsProxy(object):
    usedCompression = ""
    running = False
    zfsRoot = "/mnt/zfs"
    tempDir = "/tmp/zfsTestProxyTemp"
    zfsCacheDir = "cache"
    logFileName = "zfsd.log"
    config = os.path.join("etc", "config")
    coreDumpSettings = None
    
    def __init__(self, metaTar, zfsRoot = None,  tempDir = None,   logger = None):
        if zfsRoot:
            self.zfsRoot = zfsRoot
           
        self.logger = logger
        
        self.metaTar = metaTar
        
        if tempDir:
          self.tempDir = tempDir
        else:
          self.tempDir =  tempfile.mkdtemp(prefix = "zfsTestTemp")
          
          
    @classmethod
    def killall(self):
        self.signalAll()
        self.unmount()
        self.removeModules()
        
    @classmethod
    def signalAll(self, sigNum = None):
        if not sigNum:
            sigNum = signal.SIGKILL
        log.debug('signalling ' + str(sigNum))
        sig = Popen(args=('killall', '-' + str(sigNum), 'zfsd'), stdout=PIPE, 
                                stderr=STDOUT)
        sig.wait()
        if sig.returncode:
            log.debug("signalAll: %d output is %s", sig.returncode, sig.stdout.readlines())
        
        
            
    def collectZfsCoreDump(self, snapshot):
        #NOTE: assume core.pid format
        log.debug('collecting core dumps in %s', self.tempDir)
        if self.zfs.pid and os.path.isfile(os.path.join(self.tempDir,'core.' + str(self.zfs.pid))):
            log.debug('found core %s', 'core.' + str(self.zfs.pid))
            snapshot.addFile(name = 'zfs.core',  sourceFileName = os.path.join(self.tempDir, 'core.' + str(self.zfs.pid)), 
                                 type = SnapshotDescription.TYPE_ZFS_GCORE)
            os.unlink(os.path.join(self.tempDir, 'core.' + str(self.zfs.pid)))
            
    def installModules(self):
        modprobe = Popen(args=('modprobe', 'fuse'), stdout=PIPE, 
                                stderr=STDOUT, universal_newlines=True)
        modprobe.wait()
        if modprobe.returncode != 0:
            raise SystemError(modprobe.stdout.readlines())
            
    def makeDirs(self):
        try:
            os.makedirs(self.zfsRoot)
        except OSError: #exists
            pass
        try:
            os.makedirs(self.tempDir)
        except OSError: #exists
            pass
        
    @classmethod
    def removeModules(self):
        rm = Popen(args=('rmmod', '-f', 'fuse'), stderr = STDOUT, stdout = PIPE)
        rm.wait()
        if rm.returncode:
            log.debug("rmmod: %d output is %s", rm.returncode, rm.stdout.readlines())
        
    @classmethod
    def unmount(self):
        um = Popen(args=('umount', '-f', self.zfsRoot), stderr = STDOUT, stdout = PIPE)
        um.wait()
        if um.returncode:
            log.debug("umount: %d output is %s", um.returncode, um.stdout.readlines())
        
    def unpackData(self):
        tarFile = tarfile.open(name = self.metaTar, 
                               mode = 'r' + self.usedCompression)
        tarFile.extractall(self.tempDir)
        tarFile.close()
        
    def cleanup(self): #TODO: cleanup without tempDir removal
        shutil.rmtree(self.tempDir, True)
        
    def connectControl(self):
        if pysyplog.ping_syplog_dbus(None) != pysyplog.NOERR:
          raise Exception ("Syplog offline")
        
    def disconnectControl(self):
        pass
        
    def changeZfsLogLevel(self, loglevel):
        if not self.isRunning():
            raise Exception("zfs not running") #TODO: accurate exception
        
        pysyplog.set_level_udp(loglevel, None, 0)
        
    def isRunning(self):
        return self.running and self.zfs.poll() is None
    
    def hasDied(self):
        return self.running and self.zfs.poll()
        
    def runZfs(self):
        self.killall() #destroy previous zfsd instances
        self.makeDirs()
        self.unpackData()
        self.installModules()
        loglevel = pysyplog.LOG_LOOPS
        if self.logger:
            loglevel = pysyplog.get_log_level(self.logger)
        self.coreDumpSettings = allowCoreDumps()
        self.zfs = Popen(args=('zfsd',
                                '-d',
                                "--" + str(pysyplog.PARAM_MEDIUM_TYPE_LONG) + "=" + str(pysyplog.FILE_MEDIUM_NAME),
                                "--" + str(pysyplog.PARAM_MEDIUM_FMT_LONG) + "=" + str(pysyplog.USER_READABLE_FORMATTER_NAME),
                                "--" + str(pysyplog.PARAM_MEDIUM_FN_LONG) + "=" + os.path.join(self.tempDir, ZfsProxy.logFileName),
                                '-o', 'loglevel=' + str(loglevel) +
                                ',config=' + os.path.join(self.tempDir, self.config), 
                                self.zfsRoot),
                                cwd = self.tempDir,
                                stdout = PIPE, stderr = PIPE, universal_newlines=True)
        for i in [0.2, 0.5, 1, 3]:
            time.sleep(i)
            if zfsd_status.ping_zfsd() == zfsd_status.ZFSD_STATE_RUNNING:
              break
        if zfsd_status.ping_zfsd() != zfsd_status.ZFSD_STATE_RUNNING:
            self.killall() #be sure that we don't leave orphans
            if self.coreDumpSettings:
                setCoreDumpSettings(self.coreDumpSettings)
                self.coreDumpSettings = None
            raise Exception("Zfsd doesn't start")
        self.running = True
        self.connectControl()
        
    def stopZfs(self):
        self.disconnectControl()
        for i in [0.1, 0.5, 1]:
            try:
                os.kill(self.zfs.pid, signal.SIGTERM)
            except OSError:
                break #assume no such process
            time.sleep(i)
            if self.zfs.poll() is not None:
                break
        
        if self.zfs.poll () is None:
            os.kill(self.zfs.pid, signal.SIGKILL)
          
        self.running = False
        # to be sure that we don't leave zombies
        self.unmount()
        self.removeModules()
        if self.coreDumpSettings:
            setCoreDumpSettings(self.coreDumpSettings)
            self.coreDumpSettings = None
        
    def snapshot(self, snapshot):
        log.debug('snapshoting proxy')
        #snapshot cache
        try:
            snapshot.addDir(name = 'zfsCache',  
                        sourceDirName = os.path.join(self.tempDir, self.zfsCacheDir),
                        type = SnapshotDescription.TYPE_ZFS_CACHE)
        except (OSError, IOError):
            #ignore no dir errors, etc
            log.debug("can't snapshot zfsCache dir: %s", format_exc())
            pass
        #snapshot log
        #NOTE: this could fail when zfsd doesn't start yet
        try:
            snapshot.addFile(name = 'zfsd.log',
                         sourceFileName = os.path.join(self.tempDir, self.logFileName),
                         type = SnapshotDescription.TYPE_ZFS_LOG)
        except IOError:
            pass

        if self.isRunning():
        #snapshot zfs process
            if not self.zfs.returncode:
                gdb = Popen(args=('gdb', '-p',  str(self.zfs.pid), ), stdin = PIPE, 
                                        stdout=PIPE, stderr=PIPE, universal_newlines=True)
                gdb.stdin.write('gcore ' + self.tempDir + os.sep + 'zfsd.core.' + str(self.zfs.pid) + '\n')
                gdb.stdin.write('quit' + '\n')
                gdb.wait()
                if gdb.returncode != 0:
                    raise SystemError(gdb.stderr.readlines())
                snapshot.addFile(name = 'zfs.core',  sourceFileName = self.tempDir + os.sep + 'zfsd.core.' + str(self.zfs.pid), 
                                 type = SnapshotDescription.TYPE_ZFS_GCORE)
                         
            #set as unresumable
            snapshot.addEntry('canResume', 
                          (SnapshotDescription.TYPE_BOOL, False))
        else:
            self.collectZfsCoreDump(snapshot)
            
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
    def setupClass(self):
        log.debug("setupClass")
        config = getattr(self, zfsConfig.ZfsConfig.configAttrName)
        self.zfsRoot = config.get("global", "zfsRoot")
        self.zfsMetaTar = config.get("global", "zfsMetaTar")

        self.zfs = ZfsProxy(zfsRoot = self.zfsRoot,  metaTar = self.zfsMetaTar)
    
    def setup(self):
        log.debug("setup")
        self.zfs.runZfs()
        
    def teardown(self):
        log.debug("teardown")
        #TODO: raise exception if zfs is down
        self.zfs.stopZfs()
        self.zfs.cleanup()
        
    @classmethod
    def teardownClass(self):
        log.debug("teardownClass")
        # self.zfs = None
    
    def snapshot(self, snapshot):    
        log.debug('snapshoting test')
        # snapshot zfs
        if getattr(self, 'zfs', None):
            self.zfs.snapshot(snapshot)
            snapshot.addObject("zfsProxy", self.zfs)
        #snapshot.addObject("test",self)
        
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
        
    def raiseExceptionIfDied(self):
        if self.zfs.hasDied():
            log.debug("zfs has died")
            self.zfs.stopZfs()
            raise ZfsRuntimeException("Zfsd died upon test execution")
        

class ZfsStressTest(ZfsTest):
    metaTest = True
    definitionType = graph.GraphBuilder.USE_FLAT
    
    def __init__(self):
        ZfsTest.__init__(self)
    
    # we don't want to reset state after every test
    def setup(self):
        self.raiseExceptionIfDied()
        log.debug("stress setup")
        pass
        
    def teardown(self):
        self.raiseExceptionIfDied()
        log.debug("stress teardown")
        pass
        
    # do the before test setup only once before all tests
    @classmethod
    def setupClass(self):
        super(ZfsStressTest,self).setupClass()
        log.debug("stres setupClass")
        self.zfs.runZfs()
        
    # do the after test cleanup only once after all tests
    @classmethod
    def teardownClass(self):
        self.zfs.stopZfs()
        self.zfs.cleanup()
        log.debug("stress teardownClass")
        #super(ZfsStressTest,self).teardownClass()
        
    
