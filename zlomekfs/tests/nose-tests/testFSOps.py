##
# Test bundles of operations and check, if the result is on both files
##

import os
import pickle

from random import Random
from zfs import ZfsStressTest, abortDeadlock

from insecticide.graph import GraphBuilder
from insecticide.zfsConfig import ZfsConfig
from insecticide.timeoutPlugin import timed

class TestFSOps(ZfsStressTest):
  disabled = False
  metaTest = True
  definitionType = GraphBuilder.USE_FLAT
  
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
  
  @classmethod
  def setupClass(self):
    super(TestFSOps,self).setupClass()
    config = getattr(self,ZfsConfig.configAttrName)
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
    super(TestFSOps,self).teardownClass()
    self.cleanFiles()
  
  @classmethod
  def prepareFiles(self):
    try:
      os.mkdir(self.safeRoot + os.sep + self.safeSubdirName, True)
    except OSError:
      pass #already exists
  
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
      
  @timed(10, abortDeadlock)
  def testWriteRead(self):
    try:
        self.testFile = open(self.testFileName, 'w+')
        
        pickle.dump(self.dataVector,  self.testFile)
        self.testFile.flush()
        
        self.testFile.seek(0)
        
        self.test_data = pickle.load(self.testFile)
    except IOException:
        # could be timeout
        pass
    
    self.raiseExceptionIfDied()
    
    assert self.dataVector == self.test_data
  
  @timed(10, abortDeadlock)
  def testWriteReadonly(self):
    fd = os.open(self.testFileName,  os.O_CREAT | os.O_RDONLY)
    self.testFile = os.fdopen(fd)
    
    try:
        pickle.dump(self.dataVector,  self.testFile)
        raise Exception('Reached','Unreachable branch reached.')
    except IOError:
        print 'everything is o.k., can\'t write'
    finally:
        self.raiseExceptionIfDied()
