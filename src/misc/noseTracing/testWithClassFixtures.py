import random

def setup_module(module):
	print "setup for module " + __name__
	return

def teardown_module(module):
	print "teardown for module " + __name__
	return

class TestClass():
    
    def setup_method(self,  method = None):
        print "setup for method in class " + self.__class__.__name__
        return
    
    def teardown_method(self,  method = None):
        print "teardown for method in class " + self.__class__.__name__
        return
    
    def test_fail(self):
        print "fail test"
        assert False
    
    def test_pass(self):
        # make sure the shuffled sequence does not lose any elements
        print "pass test"
        assert True

class TestClassFixtures(TestClass):
    @classmethod
    def setup_class(self):
        print "setup for class " + self.__name__
        return

    @classmethod
    def teardown_class(self):
        print "teardown for class " + self.__name__
        return
    
if __name__ == '__main__':
    unittest.main()
