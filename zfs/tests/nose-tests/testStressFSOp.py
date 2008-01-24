##
# Tests for FS operations, should be used as metatests


import nose
import logging

import os
import random
import shutil
from insecticide import zfsConfig
from insecticide.graph import GraphBuilder
from random import Random
from nose import config
from zfs import ZfsStressTest
from traceback import format_exc
import pickle
from testFSOp import tryTouch, tryUnlink, tryRename, tryRead, tryWrite

log = logging.getLogger ("nose.tests.testStressFSOp")

class testStressFSOp(ZfsStressTest):
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
  
  @classmethod
  def prepareFiles(self):
    try:
      os.mkdir(self.safeRoot + os.sep + self.safeSubdirName, True)
    except IOError:
      pass
    except OSError:
      pass
  
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
  
  @classmethod
  def generateRandomFileName(self):
    allowedChars = 'abcdefghijklmnopqrstuvwxyz0123456789-_.'
    min = 5
    max = 15
    total = 1000000
    newName  = ''
    for count in xrange(1,total):
        for x in random.sample(allowedChars,random.randint(min,max)):
            newName += x
            
    return newName
  
  def testGenerateName(self):
    name = self.generateRandomFileName()
    self.safeFileName = self.safeRoot + os.sep + self.safeSubdirName + os.sep + name
    self.testFileName = self.testFileName = self.zfsRoot + os.sep + "bug_tree" + os.sep + name
  testGenerateName.metaTest = True
    
  def testTouch(self):
    assert tryTouch(self.safeFileName) == tryTouch(self.testFileName)
  testTouch.metaTest = True

  def testUnlink(self):
    assert tryUnlink(self.safeFileName) == tryUnlink(self.testFileName)
  testUnlink.metaTest = True
  
  def testRename(self):
    safeResult = False
    testResult = False
    
    newName = self.generateRandomFileName()
    newSafeFileName = self.safeRoot + os.sep + self.safeSubdirName + os.sep + newName
    newTestFileName = self.testFileName = self.zfsRoot + os.sep + "bug_tree" + os.sep + newName
    
    if tryRename(self.safeFileName,  newSafeFileName):
      self.safeFileName = newSafeFileName
      safeResult = True
    
    if tryRename(self.testFileName,  newTestFileName):
      self.testFileName = newTestFileName
      testResult = True
    
    assert safeResult == testResult
  testRename.metaTest = True
  
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
  testRename.metaTest = True
    
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
  testClose.metaTest = True
  
  def testRead(self):
    safeResult = tryRead(self.safeFile)
    testResult = tryRead(self.testFile)
    
    assert safeResult == testResult
  testRead.metaTest = True
  
  def testWrite(self):
    assert tryWrite(self.safeFile,  self.dataVector) == \
           tryWrite(self.testFile,  self.dataVector)
  testWrite.metaTest = True
  
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

