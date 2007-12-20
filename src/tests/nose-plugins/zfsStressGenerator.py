import logging
import unittest
import textwrap
import os

from optparse import OptionConflictError
from warnings import warn

from nose.case import FunctionTestCase,  MethodTestCase,  TestBase
from nose.suite import ContextSuiteFactory, ContextList
from nose.util import try_run
from nose.config import Config
from nose.util import tolist
from nose.plugins import Plugin

from graph import GraphBuilder

log = logging.getLogger ("nose.plugins.zfsStressGenerator")

fixtureMethods = [
                               'setup', 'setUp',  'setup_method', 
                               'teardown', 'tearDown', 'teardonw_method',
                               'setup_class',  'setup_all', 'setupClass', 
                               'setupAll', 'setUpClass',  'setUpAll', 
                               'teardown_class', 'teardown_all', 'teardownClass',
                               'teardownAll', 'tearDownClass', 'tearDownAll' 
                             ]

class MetaTestCollector(object):
    map = {}
    
    def isMetaClass (self,  cls):
        return getattr(cls, StressGenerator.metaAttrName,  False)
    
    def add(self,  method):
        classRow = self.map.get(method.im_class,  [])
        classRow.append(method)
        self.map[method.im_class] = classRow
    
    def getClassMethods(self,  cls):
        return self.map.get(cls,  None)

class StressGenerator(Plugin):
#FIXME: be robust against invalid meta classes (and report failures)
    """ ZFS stress test generator.
        testsByClass - how many stress tests will be generated from one class
        maxTestLength - maximum length of stress test (how many metatest executions)
        stopProbability - probability of terminating stress test after any metatest (in addition to dependencyGraph)
            0.1 means 10% to stop
    """
    # enable related variables
    can_configure = False
    enabled = False
    enableOpt = None
    
    # plugin name
    name = "ZfsStressGenerator"

    # to be sure to run AFTER attrib plugin
    score = 0
    
    # maxTestLength related variables
    # option string for passing max stress test length to plugin
    maxTestLengthOpt = "--stressTestLength"
    # environment variable from which default max stress test length
    # should be read
    maxTestLengthEnvOpt = "ZFS_STRESS_TEST_LENGTH"
    # max length of stress test (in meta test executions)
    maxTestLength = 100
    
    # unconfigurable variables
    testsByClass = 1
    stopProbability = 0
    
    # name of attribute which says if test is meta
    metaAttrName = "metaTest"
    metaTestCollector = MetaTestCollector()

    def __init__(self):
        Plugin.__init__(self)
    
    def addOptions(self, parser, env=os.environ):
        #for backward conpatibility
        self.add_options(parser, env)
        
    def add_options(self, parser, env=os.environ):
        """Non-camel-case version of func name for backwards compatibility.
        """
        # FIXME raise deprecation warning if wasn't called by wrapper 
        try:
            self.options(parser, env)
            self.can_configure = True
        except OptionConflictError, e:
            warn("Plugin %s has conflicting option string: %s and will "
                 "be disabled" % (self, e), RuntimeWarning)
            self.enabled = False
            self.can_configure = False
            
    def options(self, parser, env=os.environ):
        """New plugin API: override to just set options. Implement
        this method instead of addOptions or add_options for normal
        options behavior with protection from OptionConflictErrors.
        """
        Plugin.options(self,  parser,  env)
        
        # add option for max stress test length
        parser.add_option(self.maxTestLengthOpt,
                          dest=self.maxTestLengthOpt, metavar="test_count", 
                          action="store", type="int", 
                          default=env.get(self.maxTestLengthEnvOpt),
                          help="Maximal number of meta test executions in one stress test."
                                "%s (see %s) [%s]" %
                          (self.__class__.__name__, self.__class__.__name__, self.maxTestLengthEnvOpt))
        
    
    def configure(self, options, conf):
        """Configure the plugin and system, based on selected options.
        
        The base plugin class sets the plugin to enabled if the enable option
        for the plugin (self.enableOpt) is true.
        """
        Plugin.configure(self,  options,  conf)
        if not self.can_configure:
            return
        self.conf = conf
        if hasattr(options, self.enableOpt):
            self.enabled = getattr(options, self.enableOpt)
        
        if self.enabled == False:
            return
        
        # try to get name of file containing tests config
        if hasattr(options,  self.maxTestLengthOpt):
            self.maxTestLength = getattr(options,  self.maxTestLengthOpt,  self.maxTestLength)
        
    
    def help(self):
        """Return help for this plugin. This will be output as the help
        section of the --with-$name option that enables the plugin.
        """
        if self.__class__.__doc__:
            # doc sections are often indented; compress the spaces
            return textwrap.dedent(self.__class__.__doc__)
        return "(no help available)"
    
    def wantMethod(self,  method):
        if hasattr(method,  self.metaAttrName):
            isMeta = getattr(method,  self.metaAttrName)
        else: #collect all methods from "metaClasses" except fixtures
            isMeta = self.metaTestCollector.isMetaClass(method.im_class) and \
                        method.__name__ not in fixtureMethods
        # prevent meta tests to be collected
        if isMeta:
            self.metaTestCollector.add(method)
            log.debug("collecting meta test %s",  method)
            return False
        return
        
    
    def loadTestsFromTestClass(self,  cls):
        log.debug("generating stress tests from class %s methods", cls)
        allowedMethods = self.metaTestCollector.getClassMethods(cls)
        log.debug("allowedMethods %s?",allowedMethods)
        
        if allowedMethods:
            return self.generateFromClass(cls, allowedMethods)
        else:
            log.debug("no meta tests in class %s",  cls)
        
    
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
            
            while length < self.maxTestLength and next:
                methodSequence.append(getattr(cls, next))
                length += 1
                next = classGraph.next(self.stopProbability)
            if methodSequence:
                for wrapper in self.wrapMethodSequence(cls, methodSequence): 
                    tests.append(wrapper)
        
        if tests:
            log.debug("returning tests %s", tests)
            return tests
        return None
        
    def wrapMethodSequence(self, cls, methodSequence):
        log.debug("wrapping method sequence %s for stress testing"
                        "from class %s",  methodSequence,  cls)
        inst = cls()
        for i in range(0, len(methodSequence)):
            yield ChainedTestCase(method = methodSequence[i], instance = inst,  chain = methodSequence,  index = i)
        
        
        """
        return FunctionTestCase(
                                StressRun(context = cls,  methodSequence = methodSequence)
                                )
        """
    
    def handleFailure(self, test, err):
        testInst = getattr(test, "test", None)
        if not testInst:
            log.error("unexpected attr in handleFailure,  doesn't have test attr")
            return
        else:
            if testInst.__class__ is ChainedTestCase:
                log.debug("catched stress test failure (%s)",  testInst)
                log.debug("chain is %s,  index %d",  testInst.chain,  testInst.index)
                #TODO: register
                #TODO: forward to prunner
                return True
            else:
                log.debug("catched non-stress failure (%s)",  testInst)
                return False


class ChainedTestCase(MethodTestCase):
    
    def snapshotChain(self, snapshot):
        snapshot.addObject("stressChain", self.chain)
        snapshot.addEntry("stressChainIndex", 
                          (SnapshotDescription.TYPE_INT, self.index))
        
        if getattr(self, "snapshotInstFunc", None):
            self.snapshotInstFunc(self.inst, snapshot)
        
    def resumeChain(self, snapshot):
        if getattr(self, "resumeInstFunc", None):
            self.snapshotInstFunc(self.inst, snapshot)
            
        self.index = snapshot.getEntry("stressChainIndex")
        self.chain = snapshot.getObject("stressChain")
        
    def __init__(self, method, test=None, arg=tuple(), descriptor=None, instance = None,  chain = None,  index = 0):
        self.method = method
        self.test = test
        self.arg = arg
        self.descriptor = descriptor
        self.cls = method.im_class
        if instance is None:
            instance = self.cls()
        self.inst = instance
        
        # redirect 
        self.snapshotInstFunc = getattr(self.inst, "snapshot", None)
        setattr(self.inst, "snapshot", self.snapshotChain)
        
        self.resumeInstFunc = getattr(self.inst, "resume", None)
        setattr(self.inst, "resume", self.resumeChain)
        
        if self.test is None:
            method_name = self.method.__name__
            self.test = getattr(self.inst, method_name)            
        self.chain = chain
        self.index = index
        TestBase.__init__(self)
        
    def shortDescription(self):
        return self.method.__name__ + " in chain %s" % self.chain
    
