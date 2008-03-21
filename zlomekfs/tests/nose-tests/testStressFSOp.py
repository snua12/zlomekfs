##
# Tests for FS operations, should be used as metatests


import logging

import os
import time
import random

from traceback import format_exc

from insecticide.graph import GraphBuilder

from zfs import ZfsStressTest, abortDeadlock, forceDeleteFile
from testFSOp import TestFSOp, tryRead, tryWrite, tryTouch, tryUnlink, tryRename, trySeek, tryGetSize, tryGetPos


from insecticide.timeoutPlugin import timed

log = logging.getLogger ("nose.tests.testStressFSOp")

class TestGlobalState(object):
    def __init__(self):
        self.safeFile = None
        self.testFile = None
        
            
    def clean(self):
        if self.safeFile:
            forceDeleteFile(self.safeFile)
            self.safeFile = None
            
        if self.testFile:
            forceDeleteFile(self.testFile)
            self.testFile = None

class testStressFSOp(ZfsStressTest, TestFSOp):
  disabled = False
  definitionType = GraphBuilder.USE_GLOBAL
  globalState = None

    
  
  noFileSuccessors = [('testTouch', 3), ('testGenerateName', 1), ('testOpen', 5)]
  fileExistSuccessors = [('testRename', 1), ('testUnlink', 1),
                    ('testOpen', 1), ('testTouch', 1), ('testGenerateName', 1)]
  openedFileSuccessors = [('testClose', 1), ('testRead', 1), ('testWrite', 1), ('testSeek', 1), ('testGetPos', 1), ('testGetSize', 1)]
  
  graph = {
                'testGenerateName' : noFileSuccessors,
                'testTouch' : fileExistSuccessors,
                'testRename' : fileExistSuccessors,
                'testUnlink' : noFileSuccessors,
                'testOpen' : openedFileSuccessors,
                'testClose' : fileExistSuccessors,
                'testRead' : openedFileSuccessors,
                'testWrite' : openedFileSuccessors,
                'testSeek': openedFileSuccessors,
                'testGetSize': openedFileSuccessors,
                'testGetPos' : openedFileSuccessors,
                }
  startingPoint = "testGenerateName"
  
  def __init__(self):
    ZfsStressTest.__init__(self)
  
  ##
  # setup before every test method
  @classmethod
  def setupClass(self):
    self.globalState = TestGlobalState()
    super(testStressFSOp,self).setupClass()
  
  ##
  # cleanup after every test method
  @classmethod
  def teardownClass(self):
    self.cleanFiles()
    self.globalState = None
    super(testStressFSOp,self).teardownClass()
  
  def setup(self):
    ZfsStressTest.setup(self)
  
  def teardown(self):
    ZfsStressTest.teardown(self)
    
  ##
  # remove files and clean handles
  @classmethod
  def cleanFiles(self):
    log.debug('cleaning')
    self.globalState.clean()
    
    super(testStressFSOp,self).cleanFiles()
        
  @timed(2, abortDeadlock)
  def testGenerateName(self):
    name = self.generateRandomFileName()
    self.safeFileName = os.path.join(self.safeRoot, name)
    self.testFileName = os.path.join(self.zfsRoot, self.zfsVolumeDir, name)
  #testGenerateName.disabled = True
  
  @timed(5, abortDeadlock)  
  def testTouch(self):
    
    safe = tryTouch(self.safeFileName)
    test =  tryTouch(self.testFileName)
    self.raiseExceptionIfDied()
    assert safe == test
    
    @timed(1, abortDeadlock)
    def testSkip(self):
        time.sleep(0.1)
    
  @timed(10, abortDeadlock)  
  def testUnlink(self):
    safe = tryUnlink(self.safeFileName) 
    test = tryUnlink(self.testFileName)
    
    self.raiseExceptionIfDied()
    assert safe == test
  
  @timed(10, abortDeadlock)  
  def testRename(self):
    safeResult = False
    testResult = False
    
    newName = self.generateRandomFileName()
    newSafeFileName = os.path.join(self.safeRoot, newName)
    newTestFileName = os.path.join(self.zfsRoot, self.zfsVolumeDir, newName)
    
    if tryRename(self.safeFileName,  newSafeFileName):
      self.safeFileName = newSafeFileName
      safeResult = True
    
    if tryRename(self.testFileName,  newTestFileName):
      self.testFileName = newTestFileName
      testResult = True
    
    self.raiseExceptionIfDied()
    assert safeResult == testResult
  
  @timed(5, abortDeadlock)  
  def testOpen(self):
    safeResult = False
    testResult = False
    
    try:
      self.globalState.safeFile = open(self.safeFileName,  self.fileAccessMode)
      safeResult = True
    except:
      log.debug(format_exc())
      pass
    
    try:
      self.globalState.testFile = open(self.testFileName,  self.fileAccessMode)
      testResult = True
    except:
      log.debug(format_exc())
      pass
    
    self.raiseExceptionIfDied()
    assert safeResult == testResult
    
  @timed(5, abortDeadlock)  
  def testClose(self):
    safeResult = False
    testResult = False
    
    try:
      if self.globalState.safeFile:
        self.globalState.safeFile.close()
      self.globalState.safeFile = None
      safeResult = True
    except:
     log.debug(format_exc())
     pass
    
    try:
      if self.globalState.testFile:
        self.globalState.testFile.close()
      self.globalState.testFile = None
      testResult = True
    except:
     log.debug(format_exc())
     pass
    
    self.raiseExceptionIfDied()
    assert testResult == safeResult
  
  @timed(10, abortDeadlock)  
  def testRead(self):
    pos = tryGetPos(self.globalState.safeFile)
    len = tryGetSize(self.globalState.safeFile)
    
    safeResult = tryRead(self.globalState.safeFile, len - pos)
    testResult = tryRead(self.globalState.testFile, len - pos)
    
    self.raiseExceptionIfDied()
    assert safeResult == testResult
  
  @timed(15, abortDeadlock)  
  def testWrite(self):
    data = str(self.dataVector)
    log.debug('writing %d bytes into safe', len(data))
    safe = tryWrite(self.globalState.safeFile,  data)
    log.debug('writing %d bytes into test', len(data))
    test = tryWrite(self.globalState.testFile,  data)
    
    self.raiseExceptionIfDied()
    assert safe == test
    
  @timed(15, abortDeadlock)
  def testGetSize(self):
        safeSize = tryGetSize(self.globalState.safeFile)
        testSize = tryGetSize(self.globalState.testFile)
        self.raiseExceptionIfDied()
        assert safeSize == testSize
        
  @timed(10, abortDeadlock)
  def testGetPos(self):
    safePos = tryGetPos(self.globalState.safeFile)
    testPos = tryGetPos(self.globalState.testFile)
    
    self.raiseExceptionIfDied()
    assert safePos == testPos
  
  @timed(10, abortDeadlock)
  def testSeek(self):    
    # we don't get size of tested filesystem file - it is tested by testGetSize
    safeSize = tryGetSize(self.globalState.safeFile)
    newPos = random.randint(0, safeSize)
    
    safe = trySeek(self.globalState.safeFile, safeSize) 
    test = trySeek(self.globalState.testFile, safeSize)
    
    self.raiseExceptionIfDied()
    assert safe == test
    
  #TODO: implement
  def testFlush(self):
    return
  testFlush.disabled = True
  
  def testMknod(self):
    return
  testMknod.disabled = True
  
  def testGetpos(self):
    return
  testGetpos.disabled = True
  
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

