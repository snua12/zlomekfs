##
# Test bundles of operations and check, if the result is on both files
##

import nose
import os
import shutil
from random import Random
import pickle

class TestFile(object):
    
    ##
    # "safe" file for checking
    safe_file = None
    safe_file_name = "/tmp/safefile" 
    ##
    # file on test
    test_file = None
    test_file_name = "/tmp/testfile"
    
    ##
    # random data generator
    generator = Random()
    ##
    # test vector - data to write, if insufficient, 
    # they go forever
    data_vector  = []
    data_vector_length = 1024
    ##
    # setup before every test method
    def setup_method(self):
        self.clean_files()
        self.randomize_data()
    
    ##
    # cleanup after every test method
    def teardown_method(self):
        self.clean_files()
    ##
    # setup class
    def setup_class(self):
        self.generator.seed() 
    
    ##
    # remove files and clean handles
    def clean_files(self):
    
        if self.safe_file != None:
            self.safe_file.close()
            os.remove(self.safe_file_name)
        
        if self.test_file != None:
            self.test_file.close()
            os.remove(self.test_file_name)
    
    ##
    # generate random data for tests
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
