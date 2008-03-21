##
# Test bundles of operations and check, if the result is on both files
##

import os
import pickle

from random import Random
from zfs import ZfsStressTest, abortDeadlock, forceDeleteFile
from testFSOp import TestFSOp

from insecticide.graph import GraphBuilder
from insecticide.zfsConfig import ZfsConfig
from insecticide.timeoutPlugin import timed

class TestGlobalState(object):
    def __init__(self):
        self.testFile = None
    
    def clean(self):
        if self.testFile:
            forceDeleteFile(self.testFile)
        self.testFile = None
    

class TestFSOps(ZfsStressTest, TestFSOp):
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
  
  globalState = None
  
  @classmethod
  def setupClass(self):
    super(TestFSOps,self).setupClass()
    config = getattr(self,ZfsConfig.configAttrName)
    
    self.testFileName = self.zfsRoot + os.sep + "bug_tree" + os.sep + "testfile"
    self.globalState = TestGlobalState()
    
    self.generator.seed()
    self.randomizeData()
    self.prepareFiles()
  
  ##
  # cleanup after every test method
  @classmethod
  def teardownClass(self):
    self.cleanFiles()
    self.globalState = None
    super(TestFSOps,self).teardownClass()
  
  
  ##
  # remove files and clean handles
  @classmethod
  def cleanFiles(self):
    self.globalState.clean()
    
      
  @timed(10, abortDeadlock)
  def testWriteRead(self):
    try:
        self.globalState.testFile = open(self.testFileName, 'w+')
        
        pickle.dump(self.dataVector,  self.globalState.testFile)
        self.globalState.testFile.flush()
        
        self.globalState.testFile.seek(0)
        
        self.test_data = pickle.load(self.globalState.testFile)
        self.globalState.testFile.close()
    except IOError:
        # could be timeout
        pass
    
    self.raiseExceptionIfDied()
    
    assert self.dataVector == self.test_data
  
  @timed(10, abortDeadlock)
  def testWriteReadonly(self):
    fd = os.open(self.testFileName,  os.O_CREAT | os.O_RDONLY)
    self.globalState.testFile = os.fdopen(fd)
    
    try:
        pickle.dump(self.dataVector,  self.globalState.testFile)
        raise Exception('Reached','Unreachable branch reached.')
    except IOError:
        print 'everything is o.k., can\'t write'
    finally:
        self.globalState.testFile.close()
        self.raiseExceptionIfDied()
