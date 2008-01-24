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
from testFSOp import tryTouch, tryUnlink, tryRename, tryRead, tryWrite

log = logging.getLogger ("nose.tests.testStressFSOp")

class testStressFSOp(ZfsStressTest):
  disabled = False
  definitionType = GraphBuilder.USE_FLAT
  zfs = True
  
  
  
  def __init__(self):
    ZfsStressTest.__init__(self)
  
  ##
  # suffix to append when try to rename file
  file_name_suffix = ".renamed"
  
  ##
  # mode for file opening
  file_access_mode = "w"
  
  ##
  # file mode for chmod
  file_mode = 666
  
  ##
  # file owner
  file_owner = "root:root"
  
  ##
  # random data generator
  generator = Random()
  ##
  # test vector - data to write, if insufficient, 
  # they go forever
  data_vector  = []
  data_vector_length = 1024
  
  safe_file = None
  test_file = None
  safe_subdir_name = 'safedir'
  
  ##
  # setup before every test method
  @classmethod
  def setup_class(self):
    super(testStressFSOp,self).setup_class()
    config = getattr(self,zfsConfig.ZfsConfig.configAttrName)
    self.safeRoot = config.get("global","testRoot")
    self.safe_file_name = self.safeRoot + os.sep + self.safe_subdir_name + os.sep + "testfile"
    
    self.test_file_name = self.zfsRoot + os.sep + "bug_tree" + os.sep + "testfile"
 
    self.generator.seed()
    self.randomize_data()
    self.prepare_files()
  
  ##
  # cleanup after every test method
  @classmethod
  def teardown_class(self):
    super(testStressFSOp,self).teardown_class()
    self.clean_files()
  
  def setup(self):
    ZfsStressTest.setup(self)

  
  def teardown(self):
    ZfsStressTest.teardown(self)
  
  @classmethod
  def prepare_files(self):
    try:
      os.mkdir(self.safeRoot + os.sep + self.safe_subdir_name, True)
    except IOError:
      pass
    except OSError:
      pass
  
  ##
  # remove files and clean handles
  @classmethod
  def clean_files(self):
  # TODO: this wont' work since it is classmethod
    if self.safe_file != None:
      try:
        self.safe_file.close()
      except IOError:
        pass
      self.safe_file = None
    
    if self.test_file != None:
      try:
        self.test_file.close()
      except IOError:
        pass
      self.test_file = None
    
    import shutil
    shutil.rmtree(self.safeRoot + os.sep + self.safe_subdir_name, True)
  
  ##
  # generate random data for tests
  @classmethod
  def randomize_data(self):
    for i in range(self.data_vector_length):
      self.data_vector.append(self.generator.random())
    
    
  def testTouch(self):
    assert tryTouch(self.safe_file_name) == tryTouch(self.test_file_name)
  testTouch.metaTest = True

  def testUnlink(self):
    assert tryUnlink(self.safe_file_name) == tryUnlink(self.test_file_name)
  testUnlink.metaTest = True
  
  def testRename(self):
    safeResult = False
    testResult = False
    
    if tryRename(self.safe_file_name,  self.safe_file_name + self.file_name_suffix):
      self.safe_file_name = self.safe_file_name + self.file_name_suffix
      safeResult = True
    
    if tryRename(self.test_file_name,  self.test_file_name + self.file_name_suffix):
      self.test_file_name = self.test_file_name + self.file_name_suffix
      testResult = True
    
    assert safeResult == testResult
  testRename.metaTest = True
  
  def testOpen(self):
    safeResult = False
    testResult = False
    
    try:
      self.safe_file = open(self.safe_file_name,  self.file_access_mode)
      safeResult = True
    except:
      log.debug(format_exc())
      pass
    
    try:
      self.test_file = open(self.test_file_name,  self.file_access_mode)
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
      if self.safe_file:
        self.safe_file.close()
      self.safe_file = None
      safeResult = True
    except:
     log.debug(format_exc())
     pass
    
    try:
      if self.test_file:
        self.test_file.close()
      self.test_file = None
      testResult = True
    except:
     log.debug(format_exc())
     pass
    
    assert testResult == safeResult
  testClose.metaTest = True
  
  def testRead(self):
    safeResult = tryRead(self.safe_file)
    testResult = tryRead(self.test_file)
    
    assert safeResult == testResult
  testRead.metaTest = True
  
  def testWrite(self):
    assert tryWrite(self.safe_file,  self.data_vector) == \
           tryWrite(self.test_file,  self.data_vector)
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

