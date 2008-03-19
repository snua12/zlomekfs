""" Module with base classes to inherit from when creating tests for zlomekfs. """

import signal
import pysyplog
import tarfile
import tempfile
import os
import shutil
import zfsd_status  
import time
import datetime
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
    """ Proxy object which defines basic operations for zlomekfs filesystem,
        such as start (mount), stop (umount), etc.
    """
    
    usedCompression = ""
    """ How zfsMeta.tar is compressed (no compression) """
    
    running = False
    """ If zfs is running (our estimate - was started && not stoped) """
    
    zfsRoot = "/mnt/zfs"
    """ Where to mount zfs to. """
    
    tempDir = "/tmp/zfsTestProxyTemp"
    """ Temporary ditectory where to unpack data needed
        (such as zfsMeta.tar with configuration, logs, etc)
    """
    
    zfsCacheDir = "cache"
    """ Name of directory, where zfs should put it's cache """
    
    logFileName = "zfsd.log"
    """ File name for log from zfsd """
    
    config = os.path.join("etc", "config")
    """ File name of zfsd config. """
    
    coreDumpSettings = None
    """ Previous core dump settings (to restore settings after zfsd stop. """
    
    zfs = None
    """ Popen object of running zfsd process. """
    
    zfsModules = 'fuse'
    """ List of kernel modules that are needed by zfsd """
    
    def __init__(self, metaTar, zfsRoot = None,  tempDir = None,
        logger = None):
        """ Constructor for proxy.
            
            :Parameters:
                metaTar: name of file containing basic zfs data 
                    (config, cache structure)
                zfsRoot: where to mount zfs to
                tempDir: where to create dir with our session data
                logger: logging module based logger (to put logs to)
        """
        
        
        if zfsRoot:
            self.zfsRoot = zfsRoot
           
        self.logger = logger
        
        self.metaTar = metaTar
        
        if tempDir:
            self.tempDir = tempDir
        else:
            self.tempDir =  tempfile.mkdtemp(prefix = "zfsTestTemp")
          
          
    @classmethod
    def killall(cls):
        """ Kill all zfsd instances, unmount and remove modules. """
        cls.signalAll(signal.SIGKILL)
        # NOTE: this may not be correct, but we could hope :)
        cls.unmount(cls.zfsRoot)
        cls.removeModules()
        
    @classmethod
    def signalAll(cls, sigNum = None):
        """ Send given signal to all zfsd instances. 
            
            :Parameters:
                sigNum: unix signal number
        """
        
        if not sigNum:
            sigNum = signal.SIGKILL
        log.debug('signalling ' + str(sigNum))
        sig = Popen(args=('killall', '-' + str(sigNum), 'zfsd'), stdout=PIPE, 
                                stderr=STDOUT)
        sig.wait()
        if sig.returncode:
            log.debug("signalAll: %d output is %s", sig.returncode,
                sig.stdout.readlines())
            
            
            
    def collectZfsCoreDump(self, snapshot):
        """ Try to find core dump of our zfsd and append it to snapshot.
            The core dump found is deleted after append.
            
            :Parameters:
                snapshot: SnapshotDescription instance to which 
                    core dump should be inserted
        """
        
        #NOTE: assume core.pid format
        log.debug('collecting core dumps in %s', self.tempDir)
        if self.zfs.pid and os.path.isfile(os.path.join(self.tempDir,
            'core.' + str(self.zfs.pid))):
            log.debug('found core %s', 'core.' + str(self.zfs.pid))
            snapshot.addFile(name = 'zfs.core',
                sourceFileName = os.path.join(self.tempDir,
                'core.' + str(self.zfs.pid)), 
                                 type = SnapshotDescription.TYPE_ZFS_GCORE)
            os.unlink(os.path.join(self.tempDir,
                'core.' + str(self.zfs.pid)))
            
    def installModules(self):
        """ Install needed modules into kernel. """
        
        modprobe = Popen(args=('modprobe', self.zfsModules), stdout=PIPE, 
                                stderr=STDOUT, universal_newlines=True)
        modprobe.wait()
        if modprobe.returncode != 0:
            raise SystemError(modprobe.stdout.readlines())
            
    @classmethod
    def removeModules(cls):
        """ Counterpart for installModules, remove modules needed by zfsd """
        
        rmmod = Popen(args=('rmmod', '-f', 'fuse'), stderr = STDOUT, stdout = PIPE)
        rmmod.wait()
        if rmmod.returncode:
            log.debug("rmmod: %d output is %s", rmmod.returncode,
                rmmod.stdout.readlines())
        
        
    def makeDirs(self):
        """ Create dirs needed for zfsd run. """
        
        try:
            os.makedirs(self.zfsRoot)
        except OSError: #exists
            pass
        try:
            os.makedirs(self.tempDir)
        except OSError: #exists
            pass
        
    @classmethod
    def unmount(cls, path):
        """ Do unmount call on zfsRoot. """
        
        umount = Popen(args=('umount', '-f', path), stderr = STDOUT,
            stdout = PIPE)
        umount.wait()
        if umount.returncode:
            log.debug("umount: %d output is %s", umount.returncode,
                umount.stdout.readlines())
        
    def unpackData(self):
        """ Unpack zfsMeta.tar to tempDir. """
        
        tarFile = tarfile.open(name = self.metaTar, 
                               mode = 'r' + self.usedCompression)
        tarFile.extractall(self.tempDir)
        tarFile.close()
        
    def cleanup(self):
        """ Remove data used by this session (tempDir and it's content)."""
        
        shutil.rmtree(self.tempDir, True)
        
    def connectControl(self):
        """ Connect to syplog control """
        
        if pysyplog.ping_syplog_dbus(None) != pysyplog.NOERR:
            raise Exception ("Syplog offline")
        
    def disconnectControl(self):
        """ Disconnects from syplog control. """
        
        pass
        
    def changeZfsLogLevel(self, loglevel):
        """ Change logLevel of zfsd's syplog. 
            
            :Parameters:
                loglevel: log level to set
        """
        
        if not self.isRunning():
            raise ZfsRuntimeException("zfs not running")
        
        pysyplog.set_level_dbus(loglevel, None, 0)
        
    def setZfsFacility(self, facility):
        """ set zfs facility to be logged
            
            :Parameters:
                facility: facility to enable
        """
        
        if not self.isRunning():
            raise ZfsRuntimeException("zfs not running")
        
        pysyplog.set_facility_dbus(facility, None, 0)
        
    def resetZfsFacility(self, facility):
        """ set zfs facility to not be logged
            
            :Parameters:
                facility: facility to disable
        """
        
        if not self.isRunning():
            raise ZfsRuntimeException("zfs not running")
        
        pysyplog.reset_facility_dbus(facility, None, 0)
        
    def isRunning(self):
        """ Check if zfs is really running.
            
            :Return:
                True: if we thinks that zfs is running and zfsd proccess is alive.
        """
        
        return self.running and self.zfs.poll() is None and zfsd_status.ping_zfsd() == zfsd_status.ZFSD_STATE_RUNNING
    
    def hasDied(self):
        """ Check if zfs has died.
            
            :Return:
                True: if we thinks that zfs is running, but zfsd process is not alive
        """
        
        return self.running and (self.zfs.poll() or zfsd_status.ping_zfsd() != zfsd_status.ZFSD_STATE_RUNNING)
        
    def runZfs(self):
        """ Kill previously running zfsd instances and run our own zfsd. """
        
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
            "--" + str(pysyplog.PARAM_MEDIUM_TYPE_LONG) + "=" + \
                str(pysyplog.FILE_MEDIUM_NAME),
            "--" + str(pysyplog.PARAM_MEDIUM_FMT_LONG) + "=" + \
                str(pysyplog.USER_READABLE_FORMATTER_NAME),
            "--" + str(pysyplog.PARAM_MEDIUM_FN_LONG) + "=" + \
                os.path.join(self.tempDir, ZfsProxy.logFileName),
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
        """ Stop our zfsd instance. """
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
        self.unmount(self.zfsRoot)
        self.removeModules()
        if self.coreDumpSettings:
            setCoreDumpSettings(self.coreDumpSettings)
            self.coreDumpSettings = None
            
        #remove zombies
        if self.zfs:
            self.zfs.wait()
        
    def snapshot(self, snapshot):
        """ Snapshot zfsd related state (cache, core dump, etc)
            
            :Parameters:
                snapshot: SnapshotDescription instance to which data should be
                    appended
                    
        """
        
        log.debug('snapshoting proxy')
        #snapshot cache
        try:
            snapshot.addDir(name = 'zfsCache',  
                        sourceDirName = os.path.join(self.tempDir,
                            self.zfsCacheDir),
                        type = SnapshotDescription.TYPE_ZFS_CACHE)
        except (OSError, IOError):
            #ignore no dir errors, etc
            log.debug("can't snapshot zfsCache dir: %s", format_exc())
        #snapshot log
        #NOTE: this could fail when zfsd doesn't start yet
        try:
            snapshot.addFile(name = 'zfsd.log',
                         sourceFileName = os.path.join(self.tempDir,
                            self.logFileName),
                         type = SnapshotDescription.TYPE_ZFS_LOG)
        except IOError:
            pass

        if self.isRunning():
        #snapshot zfs process
            if not self.zfs.returncode:
                gdb = Popen(args=('gdb', '-p',  str(self.zfs.pid), ), 
                    stdin = PIPE, stdout=PIPE, stderr=PIPE, 
                    universal_newlines=True)
                gdb.stdin.write('gcore ' + self.tempDir + os.sep + \
                    'zfsd.core.' + str(self.zfs.pid) + '\n')
                gdb.stdin.write('quit' + '\n')
                gdb.wait()
                if gdb.returncode != 0:
                    raise SystemError(gdb.stderr.readlines())
                snapshot.addFile(name = 'zfs.core',  sourceFileName = \
                    self.tempDir + os.sep + 'zfsd.core.' + str(self.zfs.pid), 
                                 type = SnapshotDescription.TYPE_ZFS_GCORE)
                         
            #set as unresumable
            snapshot.addEntry('canResume', 
                          (SnapshotDescription.TYPE_BOOL, False))
        else:
            self.collectZfsCoreDump(snapshot)
            
        if hasattr(self,"zfs") and self.zfs.poll() is not None:
            snapshot.addObject("zfsStderr", self.zfs.stderr.readlines())
            snapshot.addObject("zfsStdout", self.zfs.stdout.readlines())
        
    def resume(self, snapshot):
        """ Resume state (if possible). This is usefull only to ease mimic of 
            environment in failure time.
            
            :Parameters:
                snapshot: SnapshotDescription instance holding zfsd state
        """
        
        try:
            (entryType, canResume) = snapshot.getEntry("canResume")
            if entryType == SnapshotDescription.TYPE_BOOL and not canResume:
                raise Exception ("snapshot is not resumable")
        except KeyError:
            pass
        

def abortDeadlock():
    log.debug("killing locked zfs in %s", str(datetime.datetime.now()))
    ZfsProxy.signalAll(signal.SIGABRT)
    #ZfsProxy.killall()
    

class ZfsTest(object):
    """ Class representing TestCase for zfsd, simple separated tests. """
    
    zfs = None
    """ ZfsProxy instance """
    
    @classmethod
    def setupClass(cls):
        """ Setup zfsd according to zfsConfig """
        
        log.debug("setupClass")
        config = getattr(cls, zfsConfig.ZfsConfig.configAttrName)
        cls.zfsRoot = config.get("global", "zfsRoot")
        cls.zfsMetaTar = config.get("global", "zfsMetaTar")

        cls.zfs = ZfsProxy(zfsRoot = cls.zfsRoot,  metaTar = cls.zfsMetaTar)
    
    def setup(self):
        """ Run zfsd for the test. """
        
        log.debug("setup")
        self.zfs.runZfs()
        
    def teardown(self):
        """ Stop zfsd after test. """
        log.debug("teardown")
        self.raiseExceptionIfDied()
        self.zfs.stopZfs()
        self.zfs.cleanup()
        
    @classmethod
    def teardownClass(cls):
        """ Do cleanup after tests (currently void) """
        
        log.debug("teardownClass")
        # self.zfs = None
    
    def snapshot(self, snapshot):    
        """ Snapshot current test.  
            Appends zfsd state and this object state information.
        """
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
        
    
    def resume(self,  snapshot):
        """ Resume test state """
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
    """ Class representing TestCase for stress testing.
        Meta test holder classes should inherit from this class.
    """
    
    metaTest = True
    """ mark this class as meta test holder. """
    
    definitionType = graph.GraphBuilder.USE_FLAT
    """ Default definition type is flat (use all tests) """
    
    def __init__(self):
        ZfsTest.__init__(self)
    
    def setup(self):
        """ Void function to override default behavior.
            We don't want to reset state after every test.
            Raise exception upon unexpected zfsd process termination.
        """
        
        self.raiseExceptionIfDied()
        log.debug("stress setup")
        
    def teardown(self):
        """ Void function to override default behavior.
            We don't want to reset state after every test.
            Raise exception upon unexpected zfsd process termination.
        """
        self.raiseExceptionIfDied()
        log.debug("stress teardown")
        
    @classmethod
    def setupClass(cls):
        """ Do the before test setup only once before all tests. """
        super(ZfsStressTest, cls).setupClass()
        log.debug("stres setupClass")
        cls.zfs.runZfs()
        
    @classmethod
    def teardownClass(cls):
        """ Do the after test cleanup only once after all tests. """
        cls.zfs.stopZfs()
        cls.zfs.cleanup()
        log.debug("stress teardownClass")
        #super(ZfsStressTest,self).teardownClass()
        
    
