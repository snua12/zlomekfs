##
# Tests for FS operations, should be used as metatests


import logging
import tempfile
import os

from traceback import format_exc
from random import Random, sample, randint

from insecticide import zfsConfig
from insecticide.snapshot import SnapshotDescription

from zfs import ZfsTest

log = logging.getLogger ("nose.tests.testFSOp")

def tryGetSize(file):
    try:
        pos = file.tell()
        file.seek(0, os.SEEK_END)
        size = file.tell()
        file.seek(pos, os.SEEK_SET)
        return size
    except os.error:
        return -1
    
def tryGetPos(file):
    try:
        return file.tell()
    except (OSError, IOError):
        return -1

def tryTouch(fileName):
  try:
    log.debug ("try to touch file %s", fileName)
    fd = os.open(fileName, os.O_WRONLY | os.O_CREAT, 0666)
    os.close(fd)
    os.utime(fileName, None)
    return True
  except (OSError, IOError):
    log.debug(format_exc())
    return False

def tryUnlink(fileName):
  try:
    log.debug ("try to unlink file %s", fileName)
    os.unlink(fileName)
    return True
  except (OSError, IOError):
   log.debug(format_exc())
   return False

def tryRename(originalFileName,  newFileName):
  try:
    log.debug ("try to rename file %s to file %s", originalFileName, newFileName)
    os.rename(originalFileName,  newFileName)
    return True
  except (OSError, IOError):
   log.debug(format_exc())
   return False

def tryRead(file, bytes):
  try:
    log.debug ("try read from file %s", file.name)
    return file.read(bytes)
  except (IOError, OSError):
   log.debug(format_exc())
   return None
    
def tryWrite(file,  data):
  try:
    log.debug ("try write to file %s", file.name)
    file.write(data)
    return True
  except (IOError, OSError):
    log.debug(format_exc())
    return False

def trySeek(file, newPos):
    try:
        log.debug("try to seek to %s", str(newPos))
        file.seek(newPos, os.SEEK_SET)
    except (IOError, OSError):
        log.debug(format_exc())
        
    return file.tell()
    
class TestFSOp(ZfsTest):
  disabled = False
  
  def __init__(self):
    ZfsTest.__init__(self)
  
  ##
  # mode for file opening
  fileAccessMode = "w"
  
  ##
  # file mode for chmod
  fileMode = 666
  
  ##
  # file owner
  fileOwner = "root:root"
  
  ##
  # random data generator
  generator = Random()
  ##
  # test vector - data to write, if insufficient, 
  # they go forever
  dataVector  = []
  dataVectorLength = 1024
  
  safeRoot = None
  safeFile = None
  testFile = None
  
  def snapshot(self, snapshot):
    if self.safeRoot:
        try:
            
            snapshot.addDir('compareDir', self.safeRoot, type = SnapshotDescription.TYPE_COMPARE_FS)
        except (OSError, IOError):
            #ignore no dir errors, etc
            log.debug("can't snapshot compare dir: %s", format_exc())
            pass
    ZfsTest.snapshot(self, snapshot)
  
  def resume(self, snapshot):
    ZfsTest.resume(self, snapshot)
    snapshot.getDir('compareDir', self.safeRoot)
    
  ##
  # setup before every test method
  @classmethod
  def setupClass(self):
    super(TestFSOp,self).setupClass()
    self.prepareFiles()
    
    self.safeFileName = os.path.join(self.safeRoot, "testfile")
    self.testFileName = os.path.join(self.zfsRoot, 'bug_tree', "testfile")
    
    self.generator.seed()
    self.randomizeData()
    log.debug(self.__name__ + "setupclass finish")
  
  ##
  # cleanup after every test method
  @classmethod
  def teardownClass(self):
    super(TestFSOp,self).teardownClass()
  
  def setup(self):
    ZfsTest.setup(self)
    self.prepareFiles()
    
  def teardown(self):
   log.debug(self.__class__.__name__ + "teardown")
   ZfsTest.teardown(self)
   self.cleanFiles()
   log.debug(self.__class__.__name__ + "teardown finish")
   
  @classmethod
  def prepareFiles(self):
    config = getattr(self,zfsConfig.ZfsConfig.configAttrName)
    globalSafeRoot = config.get("global","testRoot")
    try:
        os.makedirs(globalSafeRoot, True)
    except OSError:
        # directory exists
        pass
    self.safeRoot =  tempfile.mkdtemp(prefix = "testCompareDir",
        dir = globalSafeRoot)
  
  @classmethod
  def cleanFiles(self):
    try:
      import shutil
      shutil.rmtree(self.safeRoot)
    except KeyboardInterrupt:
      raise
    except:
      pass
  
  ##
  # generate random data for tests
  @classmethod
  def randomizeData(self):
    for i in range(self.dataVectorLength):
      self.dataVector.append(self.generator.random())
    
  @classmethod
  def generateRandomFileName(self):
    allowedChars = 'abcdefghijklmnopqrstuvwxyz0123456789-_.'
    min = 2
    max = 5
    total = 5
    newName  = ''
    for count in xrange(1,total):
        for x in sample(allowedChars,randint(min,max)):
            newName += x
            
    log.debug ("new name is " + newName)
    return newName
