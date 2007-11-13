import logging
import unittest

from graph import GraphBuilder
from nose.case import FunctionTestCase
from nose.util import try_run
from nose.config import Config

class StressGenerator(object):
    testsByClass = 1
    maxTestLength = 100
    stopProbability = 0
    log = None
    
    def __init__(self, log = None,  testsByClass=1, maxTestLength=100,  stopProbability = 0):
        """
        Init stress generator
        testsByClass - how many stress tests will be generated from one class
        maxTestLength - maximum length of stress test (how many metatest executions)
        stopProbability - probability of terminating stress test after any metatest (in addition to dependencyGraph)
            0.1 means 10% to stop
        """
        self.testsByClass = testsByClass
        self.maxTextLength = maxTestLength
        self.stopProbability = stopProbability
        if log:
            self.log = log
        else:
            self.log = logging.getLogger(__name__)
    
    def generateFromClass(self,  cls,  allowedMethods):
        if not allowedMethods:
            return
        
        methodNames = []
        for method in allowedMethods:
            methodNames.append(method.__name__)
        tests = []
        
        for i in range(0, self.testsByClass):
            classGraph = GraphBuilder.generateDependencyGraph(cls, methodNames)
            methodSequence = list()
            length = 0
            next = classGraph.next(stopProbability = self.stopProbability)
            
            while length < self.maxTextLength and next:
                methodSequence.append(getattr(cls, next))
                length += 1
                next = classGraph.next(self.stopProbability)
            if methodSequence:
                tests.append(self.wrapMethodSequence(cls, methodSequence))
        
        if tests:
            self.log.debug("returning tests %s", tests)
            return tests
        return None
        
    def wrapMethodSequence(self, cls, methodSequence):
        self.log.debug("wrapping method sequence %s for stress testing"
                        "from class %s",  methodSequence,  cls)
        
        return FunctionTestCase(
                                StressRun(context = cls,  methodSequence = methodSequence)
                                )

class StressRun(object):
    def __init__(self, context = None,  methodSequence = []):
        self.context = context
        self.methodSequence = methodSequence
        self.inst = context()
        self.__name__ = self.generateDescription()
        
    def __call__(self,  *arg,  **kwarg):
        self.runSequence()
        
    def runSequence(self):
        for method in self.methodSequence:
            self.setup_context()
            method(self.inst)
            self.teardown_context()
        
    def setup_context(self):
        names = ('setup', 'setUp', 'setUpFunc')
        try_run(self.inst, names)
        
    def teardown_context(self):
        names = ('teardown', 'tearDown', 'tearDownFunc')
        try_run(self.inst, names)
        
    def generateDescription(self):
        ret = "StressTest (%s): [ " % self.context
        for method in self.methodSequence:
            ret += method.__name__ + " "
        
        ret += "]"
        return ret
    def shortDescription(self):
        return self.__name__
