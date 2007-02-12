##
# Test I wrote to test, what I can do with py.test
##

import os
import shutil
import random

class TestFile(object):
    
    ##
    # "safe" file for checking
    safe_file = None
    safe_file_name = "/tmp/testfile" 
    ##
    # file on test
    test_file = None
    test_file_name = "./testfile"
    
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
    def setup_method(self):
        clean_files()
        randomize_data()
    
    ##
    # cleanup after every test method
    def teardown_method(self):
        clean_files()
    ##
    # setup class
    setup_class(self):
        generator.seed() 
    
    ##
    # remove files and clean handles
    def clean_files(self):
	try:
            if safe_file != None:
                safe_file.close()
        
        try:
            if test_file != None:
                test_file.close()
        os.remove(safe_file_name)
        os.remove(test_file_name)
    
    ##
    # generate random data for tests
    def randomize_data(self):
        for i in range(data_vector_length)
            data_vector[i] = generator.random()
    
    def test_write_read(self):
        safe_file = open(safe_file_name, 'w')
        test_file = open(test_file_name, 'w')
        
        safe_file.write(data_vector)
        safe_file.flush()
        test_file.write(data_vector)
        test_file.flush()
        
        safe_file.seek(0)
        test_file.seek(0)
        
        safe_data = safe_file.read()
        test_data = test_file.read()
        
        assert safe_data == test_data
    
    def test_write_readonly(self):
        test_file = open(test_file_name,'r')
        
        try:
            test_file.write(data_vector)
            raise Exception('Reached','Unreachable branch reached.')
        except IOError:
            print 'everything is o.k., can\'t write'
