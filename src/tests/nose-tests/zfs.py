from snapshot import SnapshotDescription
from subprocess import Popen, PIPE
import signal
import pysyplog
import tempfile

class ZfsProxy(object):
    zfsRoot = "/mnt/zfs"
    zfsCache = "/var/cache/zfs" # TODO: use in zfs
    tempDir = "/tmp/zfsTestProxyTemp"
    logFileName = "zfsd.log"
    
    running = False
    def __init__(self,  zfsRoot = None,  zfsCache = None, tempDir = None,  logger = None):
        if zfsRoot:
            self.zfsRoot = zfsRoot
        if zfsCache:
            self.zfsCache = zfsCache
            
        self.logger = logger
        
        if tempDir:
          self.tempDir = tempDir
        else:
          self.tempDir =  tempfile.mkdtemp(prefix = "zfsTestTemp")
    def runZfs(self):
        modprobe = subprocess.Popen(args=('modprobe', 'fuse'), stdout=PIPE, 
                                stderr=PIPE, universal_newlines=True)
        modprobe.wait()
        if modprobe.returncode != 0:
            raise Exception(modprobe.stderr.readlines()) #FIXME: accurate exception
        
        self.zfs = Popen(args=('zfsd',
                                '-o', 'loglevel=' + str(pysyplog.get_log_level(logger)),
                                '-d',
                                "--" + str(pysyplog.PARAM_MEDIUM_TYPE_LONG) + "=" + str(pysyplog.FILE_MEDIUM_NAME),
                                "--" + str(pysyplog.PARAM_FMT_LONG) + "=" + str(pysyplog.USER_READABLE_FORMATER_NAME),
                                "--" + str(pysyplog.PARAM_MEDIUM_FN_LONG) + "=" + self.tempDir + os.sep + ZfsProxy.logFileName,
                                '-f', self.zfsRoot),
                                cwd='/tmp/',
                                stdout=PIPE, stderr=PIPE, universal_newlines=True) # FIXME: core dump reporting
        
        self.running = True
        
    def stopZfs(self):
        #TODO: check status
        os.kill(self.zfs.pid, signal.SIGTERM)
        Popen(args=('umount', '-f', self.zfsRoot))
        Popen(args=('rmmod', '-f', 'fuse'))
        self.running = False
        
    def connectControl(self):
        pass
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

    def __init__(self):
        config = getattr(self, zfsConfig.zfsConfig.configAttrName)
        self.zfsRoot = config.get("global", "zfsRoot")
        self.zfsCache = config.get("global", "zfsCache")
        
    def setup_class(self):
        self.zfs = ZfsProxy(zfsRoot = self.zfsRoot,  zfsCache = self.zfsCache)
    
    def setup(self):
        self.zfs.runZfs()
        
    def teardown(self):
        #TODO: raise exception if zfs is down
        self.zfs.stopZfs()
        
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
