##
# Tests for FS operations, should be used as metatests


import nose
import logging

import os
import shutil
from insecticide import zfsConfig
from random import Random
from nose import config
from zfs import ZfsTest
from traceback import format_exc
import pickle

log = logging.getLogger ("nose.tests.testFSOp")

def tryTouch(fileName):
  try:
    fd = os.open(fileName, os.O_WRONLY | os.O_CREAT, 0666)
    os.close(fd)
    os.utime(fileName, None)
    return True
  except:
    log.debug(format_exc())
    return False

def tryUnlink(fileName):
  try:
    os.unlink(fileName)
    return True
  except:
   log.debug(format_exc())
   return False

def tryRename(originalFileName,  newFileName):
  try:
    os.rename(originalFileName,  newFileName)
    return True
  except:
   log.debug(format_exc())
   return False

def tryRead(file):
  try:
    return pickle.load(file)
  except:
   log.debug(format_exc())
   return None
    
def tryWrite(file,  data):
  try:
    pickle.dump(data,  file)
    return True
  except:
    log.debug(format_exc())
    return False

class testFSOp(ZfsTest):
  disabled = False
  zfs = True
  
  
  
  def __init__(self):
    ZfsTest.__init__(self)
  
  ##
  # suffix to append when try to rename file
  fileNameSuffix = ".renamed"
  
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
  
  safeFile = None
  testFile = None
  safeSubdirName = 'safedir'
  
  ##
  # setup before every test method
  @classmethod
  def setupClass(self):
    super(testFSOp,self).setupClass()
    config = getattr(self,zfsConfig.ZfsConfig.configAttrName)
    self.safeRoot = config.get("global","testRoot")
    self.safeFileName = self.safeRoot + os.sep + self.safeSubdirName + os.sep + "testfile"
    
    self.testFileName = self.zfsRoot + os.sep + "bug_tree" + os.sep + "testfile"
 
    self.generator.seed()
    self.randomizeData()
  
  ##
  # cleanup after every test method
  @classmethod
  def teardownClass(self):
    super(testFSOp,self).teardownClass()
  
  def setup(self):
    ZfsTest.setup(self)
    self.prepareFiles()
  
  def teardown(self):
    ZfsTest.teardown(self)
    self.cleanFiles()
  
  def prepareFiles(self):
    try:
      os.mkdir(self.safeRoot + os.sep + self.safeSubdirName, True)
    except IOError:
      pass
  
  ##
  # remove files and clean handles
  def cleanFiles(self):
  # TODO: this wont' work since it is classmethod
    if self.safeFile != None:
      try:
        self.safeFile.close()
      except IOError:
        pass
      self.safeFile = None
    
    if self.testFile != None:
      try:
        self.testFile.close()
      except IOError:
        pass
      self.testFile = None
    
    import shutil
    shutil.rmtree(self.safeRoot + os.sep + self.safeSubdirName, True)
  
  ##
  # generate random data for tests
  @classmethod
  def randomizeData(self):
    for i in range(self.dataVectorLength):
      self.dataVector.append(self.generator.random())
    
    
  def testTouch(self):
    assert tryTouch(self.safeFileName) == tryTouch(self.testFileName)

  def testUnlink(self):
    assert tryUnlink(self.safeFileName) == tryUnlink(self.testFileName)
  
  def testRename(self):
    safeResult = False
    testResult = False
    
    if tryRename(self.safeFileName,  self.safeFileName + self.fileNameSuffix):
      self.safeFileName = self.safeFileName + self.fileNameSuffix
      safeResult = True
    
    if tryRename(self.testFileName,  self.testFileName + self.fileNameSuffix):
      self.testFileName = self.testFileName + self.fileNameSuffix
      testResult = True
    
    assert safeResult == testResult
  
  def testOpen(self):
    safeResult = False
    testResult = False
    
    try:
      self.safeFile = open(self.safeFileName,  self.fileAccessMode)
      safeResult = True
    except:
      log.debug(format_exc())
      pass
    
    try:
      self.testFile = open(self.testFileName,  self.fileAccessMode)
      testResult = True
    except:
     log.debug(format_exc())
     pass
    
    assert testResult == safeResult
    
  def testClose(self):
    safeResult = False
    testResult = False
    
    try:
      if self.safeFile:
        self.safeFile.close()
      self.safeFile = None
      safeResult = True
    except:
     log.debug(format_exc())
     pass
    
    try:
      if self.testFile:
        self.testFile.close()
      self.testFile = None
      testResult = True
    except:
     log.debug(format_exc())
     pass
    
    assert testResult == safeResult
  
  def testRead(self):
    safeResult = tryRead(self.safeFile)
    testResult = tryRead(self.testFile)
    
    assert safeResult == testResult
  
  def testWrite(self):
    assert tryWrite(self.safeFile,  self.dataVector) == \
           tryWrite(self.testFile,  self.dataVector)

  def testFlush(self):
    return
  testFlush.disabled = True
  
  def testMknod(self):
    return
  testMknod.disabled = True
  
  def testGetpos(self):
    return
  testGetpos.disabled = True
  
  def testSeek(self):
    return
  testSeek.disabled = True
  
  def testTruncate(self):
    return
  testTruncate.disabled = True
  
  def testFeof(self):
    return
  testFeof.disabled = True
  
  def testGetAtime(self):
    return
  testGetAtime.disabled = True
  
  def testGetCtime(self):
    return
  testGetCtime.disabled = True
  
  def testGetMtime(self):
    return
  testGetMtime.disabled = True

  def testGetattr(self):
    return
  testGetattr.disabled = True
  
  def testSetattr(self):
    return
  testSetattr.disabled = True
  
  def testFlock(self):
    return
  testFlock.disabled = True
  
  def testFunlock(self):
    return
  testFunlock.disabled = True
  
  def testSymlink(self):
    return
  testSymlink.disabled = True
  
  def testReadlink(self):
    return
  testReadlink.disabled = True
  
  def testMkdir(self):
    return
  testMkdir.disabled = True
  
  def testRmdir(self):
    return
  testRmdir.disabled = True
  
  def testReaddir(self):
    return
  testReaddir.disabled = True
  
  def testLink(self):
    return
  testLink.disabled = True

  def testChmod(self):
    return
  testChmod.disabled = True
  
  def testChown(self):
    return
  testChown.disabled = True

  def testSetxattr(self):
    return
  testSetxattr.disabled = True
  
  def testGetxattr(self):
    return
  testGetxattr.disabled = True
  
  def testListxattr(self):
    return
  testListxattr.disabled = True
  
  def testRemovexattr(self):
    return
  testRemovexattr.disabled = True

