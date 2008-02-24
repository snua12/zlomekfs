##
# Tests for FS operations, should be used as metatests


import logging

import os
import signal
import time
from traceback import format_exc
from insecticide import zfsConfig
from insecticide.graph import GraphBuilder
from zfs import ZfsStressTest, ZfsProxy
from testFSOp import testFSOp, tryRead, tryWrite, tryTouch, tryUnlink, tryRename

from insecticide.timeoutPlugin import timed

log = logging.getLogger ("nose.tests.testStressFSOp")

def abortDeadlock():
    ZfsProxy.signalAll(signal.SIGABRT)
    #ZfsProxy.killall()

class testStressFSOp(ZfsStressTest, testFSOp):
  disabled = False
  definitionType = GraphBuilder.USE_GLOBAL
  zfs = True
  
  noFileSuccessors = [('testTouch', 3), ('testGenerateName', 1), ('testOpen', 5)]
  fileExistSuccessors = [('testRename', 1), ('testUnlink', 1),
                    ('testOpen', 1), ('testTouch', 1), ('testGenerateName', 1)]
  openedFileSuccessors = [('testClose', 1), ('testRead', 1), ('testWrite', 1)]
  
  graph = {
                'testGenerateName' : noFileSuccessors,
                'testTouch' : fileExistSuccessors,
                'testRename' : fileExistSuccessors,
                'testUnlink' : noFileSuccessors,
                'testOpen' : openedFileSuccessors,
                'testClose' : fileExistSuccessors,
                'testRead' : openedFileSuccessors,
                'testWrite' : openedFileSuccessors
                }
  startingPoint = "testGenerateName"
  
  def __init__(self):
    ZfsStressTest.__init__(self)
  
  ##
  # setup before every test method
  @classmethod
  def setupClass(self):
    super(testStressFSOp,self).setupClass()
    config = getattr(self,zfsConfig.ZfsConfig.configAttrName)
    self.safeRoot = config.get("global","testRoot")
    self.safeFileName = self.safeRoot + os.sep + self.safeSubdirName + os.sep + "testfile"
    self.testFileName = self.zfsRoot + os.sep + "bug_tree" + os.sep + "testfile"
 
    self.generator.seed()
    self.randomizeData()
    self.prepareFiles()
  
  ##
  # cleanup after every test method
  @classmethod
  def teardownClass(self):
    super(testStressFSOp,self).teardownClass()
    self.cleanFiles()
  
  def setup(self):
    ZfsStressTest.setup(self)

  
  def teardown(self):
    ZfsStressTest.teardown(self)
  
  ##
  # remove files and clean handles
  @classmethod
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
      
  
  @timed(0.5, abortDeadlock)
  def testGenerateName(self):
    name = self.generateRandomFileName()
    self.safeFileName = self.safeRoot + os.sep + self.safeSubdirName + os.sep + name
    self.testFileName = self.testFileName = self.zfsRoot + os.sep + "bug_tree" + os.sep + name
  #testGenerateName.disabled = True
  
  @timed(1, abortDeadlock)  
  def testTouch(self):
    assert tryTouch(self.safeFileName) == tryTouch(self.testFileName)
    
  @timed(5, abortDeadlock)  
  def testUnlink(self):
    assert tryUnlink(self.safeFileName) == tryUnlink(self.testFileName)
  
  @timed(5, abortDeadlock)  
  def testRename(self):
    safeResult = False
    testResult = False
    
    newName = self.generateRandomFileName()
    newSafeFileName = os.path.join(self.safeRoot , self.safeSubdirName, newName)
    newTestFileName = os.path.join(self.zfsRoot, "bug_tree", newName)
    
    if tryRename(self.safeFileName,  newSafeFileName):
      self.safeFileName = newSafeFileName
      safeResult = True
    
    if tryRename(self.testFileName,  newTestFileName):
      self.testFileName = newTestFileName
      testResult = True
    
    assert safeResult == testResult
  
  @timed(2, abortDeadlock)  
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
    
  @timed(2, abortDeadlock)  
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
  
  @timed(5, abortDeadlock)  
  def testRead(self):
    safeResult = tryRead(self.safeFile)
    testResult = tryRead(self.testFile)
    
    assert safeResult == testResult
  
  @timed(5, abortDeadlock)  
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


class testStressFSOpRandomly (testStressFSOp):
    disabled = True
    definitionType = GraphBuilder.USE_FLAT

