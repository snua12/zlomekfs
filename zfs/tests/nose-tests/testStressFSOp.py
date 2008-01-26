##
# Tests for FS operations, should be used as metatests


import nose
import logging

import os
import shutil
from insecticide import zfsConfig
from insecticide.graph import GraphBuilder
from random import Random
from nose import config
from zfs import ZfsStressTest
from traceback import format_exc
import pickle
from testFSOp import tryTouch, tryUnlink, tryRename, tryRead, tryWrite, testFSOp

log = logging.getLogger ("nose.tests.testStressFSOp")

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
      
  

class testStressFSOpRandomly (testStressFSOp):
    disabled = True
    definitionType = GraphBuilder.USE_FLAT

