from snapshot import SnapshotDescription
from subprocess import Popen, PIPE
import signal
import pysyplog

class ZfsProxy(object):
    zfsRoot = "/mnt/zfs"
    zfsCache = "/var/cache/zfs" # TODO: use in zfs
    running = False
    def __init__(self,  zfsRoot = None,  zfsCache = None,  logger = None):
        if zfsRoot:
            self.zfsRoot = zfsRoot
        if zfsCache:
            self.zfsCache = zfsCache
            
        self.logger = logger
        
    def runZfs(self):
        modprobe = subprocess.Popen(args=('modprobe', 'fuse'), stdout=PIPE, 
                                stderr=PIPE, universal_newlines=True)
        modprobe.wait()
        if modprobe.returncode != 0:
            raise Exception(modprobe.stderr.readlines()) #FIXME: accurate exception
        
        loglevel = 10 # FIXME: read from somewhere (config?)
        self.zfs = Popen(args=('zfsd', '-o', 'loglevel=' + str(pysyplog.get_log_level(logger)), '-d',
                                '-f', self.zfsRoot), cwd='/tmp/',
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
        snapshot.addDir(name = "zfsCache",  
                        sourceDirName = self.zfsCache,
                        type = SnapshotDescription.TYPE_ZFS_CACHE)
        #snapshot process
        #FIXME : snapshot zfs process
        if self.running:
            #set as unresumable
            snapshot.addEntry("canResume", 
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
