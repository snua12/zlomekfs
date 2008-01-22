##
# Test bundles of operations and check, if the result is on both files
##

import nose
import os
import shutil
from random import Random
from nose import config
import pickle
from zfs import ZfsStressTest
from insecticide.graph import GraphBuilder
from insecticide.zfsConfig import ZfsConfig

class testFSOps(ZfsStressTest):
  disabled = False
  metaTest = True
  definitionType = GraphBuilder.USE_FLAT
  
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
  
  @classmethod
  def setup_class(self):
    super(testFSOps,self).setup_class()
    config = getattr(self,ZfsConfig.configAttrName)
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
    super(testFSOps,self).teardown_class()
    self.clean_files()
  
  @classmethod
  def prepare_files(self):
    try:
      os.mkdir(self.safeRoot + os.sep + self.safe_subdir_name, True)
    except OSError:
      pass #already exists
  
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
      
  def test_write_read(self):
    self.safe_file = open(self.safe_file_name, 'w+')
    self.test_file = open(self.test_file_name, 'w+')
    
    pickle.dump(self.data_vector,  self.safe_file)
    self.safe_file.flush()
    pickle.dump(self.data_vector,  self.test_file)
    self.test_file.flush()
    
    self.safe_file.seek(0)
    self.test_file.seek(0)
    
    self.safe_data = pickle.load(self.safe_file)
    self.test_data = pickle.load(self.test_file)
    
    assert self.safe_data == self.test_data

  def test_write_readonly(self):
    fd = os.open(self.test_file_name,  os.O_CREAT | os.O_RDONLY)
    self.test_file = os.fdopen(fd)
    
    try:
        pickle.dump(self.data_vector,  self.test_file)
        raise Exception('Reached','Unreachable branch reached.')
    except IOError:
        print 'everything is o.k., can\'t write'
