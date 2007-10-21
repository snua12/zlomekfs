##
# Tests for FS operations, should be used as metatests


import nose

import os
import shutil
import random

def tryTouch(fileName):
  try:
    handle = os.path(fileName)
    handle.touch()
    return safeHandle.isFile()
  except:
    return False

def tryUnlink(fileName):
  try:
    os.unlink(fileName)
    return True
  except:
    return False

def tryRename(originalFileName,  newFileName):
  try:
    os.rename(originalFileName,  newFileName)
    return True
  except:
    return False

def tryRead(file):
  try:
    return file.read()
  except:
    return None

class testFSOp(object):
  
  ##
  # "safe" file for checking
  safe_file = None
  safe_file_name = "/tmp/testfile" 
  
  ##
  # file on test
  test_file = None
  test_file_name = "./testfile"
  
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
  generator = Random.init()
  ##
  # test vector - data to write, if insufficient, 
  # they go forever
  data_vector  = []
  data_vector_length = 1024
  
  ##
  # setup before every test method
  def setup_class(self):
    generator_seed()
    clean_files()
    randomize_data()
  
  ##
  # cleanup after every test method
  def teardown_class(self):
    clean_files()
  
  ##
  # remove files and clean handles
  def clean_files(self):
    try:
      if safe_file != None:
        safe_file.close()
    except:
      pass
    
    try:
      if test_file != None:
        test_file.close()
    except:
      pass
    
    os.remove(safe_file_name)
    os.remove(test_file_name)
    
  ##
  # generate random data for tests
  def randomize_data(self):
    for i in range(data_vector_length):
      data_vector[i] = generator.random()
    
    
  def testTouch(self):
    assert tryTouch(self.safe_file_name) == tryTouch(self.test_file_name)
#  testTouch.disabled = config.readOnly

  def testUnlink(self):
    assert tryUnlink(self.safe_file_name) == tryUnlink(self.test_file_name)
#  testUnlink.disabled = config.readOnly
  
  def testRename(self):
    safeResult = False
    testResult = False
    
    if tryRename(self.safe_file_name,  self.safe_file_name + self.file_name_suffix):
      self.safe_file_name = self.safe_file_name + self.file_name_suffix
      safeResult = True
    
    if tryRename(self.test_file_name,  self.test_file_name + self.file_name_suffix):
      self.test_file_name = self.test_file_name + test.file_name_suffix
      testResult = True
    
    assert safeResult == testResult
#  testRename.disabled = config.readOnly
  
  def testOpen(self):
    safeResult = False
    testResult = False
    
    try:
      self.safe_file = open(self.safe_file_name,  self.file_access_mode)
      safeResult = True
    except:
      pass
    
    try:
      self.test_file = open(self.test_file_name,  self.file_access_mode)
      testResult = True
    except:
      pass
    
    assert testResult == safeResult
    
  def testClose(self):
    safeResult = False
    testResult = False
    
    try:
      close(self.safe_file)
      self.safe_file = None
      safeResult = True
    except:
      pass
    
    try:
      close(self.test_file)
      test.safe_file = None
      testResult = True
    except:
      pass
    
    assert testResult == safeResult
  
  def testRead(self):
    safeResult = tryRead(self.safe_file)
    testResult = tryRead(self.test_file)
    
    assert safeResult == testResult
  
  def testWrite(self):
    assert tryWrite(self.safe_file,  self.data_vector,  self.data_vector_length) == \
                 try_write(self.test_file,  self.data_vector,  self.data_vector_length)
#  testWrite.disabled = config.readOnly

  def testFlush(self):
    return
  testFlush.disabled = True
  
  def testMknod(self):
    return
  testMknod.disabled = True
  
  def testGetpos(self):
    return
  testGetPos.disabled = True
  
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
  test.disabled = True
  
  def testRemovexattr(self):
    return
  testRemovexattr.disabled = True

