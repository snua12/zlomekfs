import logging
import unittest
import textwrap
import os

from optparse import OptionConflictError
from warnings import warn
from failure import ZfsTestFailure
from report import ReportProxy
from snapshot import SnapshotDescription

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
    testsByClass = 2
    stopProbability = 0
    useShortestPath = True
    retriesAfterFailure = 3
    
    # name of attribute which says if test is meta
    metaAttrName = "metaTest"
    metaTestCollector = MetaTestCollector()
    reportProxy = ReportProxy()

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
        self.suiteClass = ContextSuiteFactory(config=conf)
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
        
    
    def generateOneStress(self, cls, allowedMethods):
        if not allowedMethods:
            return None
        
        methodNames = []
        for method in allowedMethods:
            methodNames.append(method.__name__)
        tests = []
        
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
            suite = self.suiteClass(ContextList(tests, context=cls))
            def runWithStopSuiteOnTestFail(self, result):
                #NOTE: keep this in sync with nose.suite.ContextSuite.run
                # proxy the result for myself
                if self.resultProxy:
                    result, orig = self.resultProxy(result, self), result
                else:
                    result, orig = result, result
                try:
                    self.setUp()
                except KeyboardInterrupt:
                    raise
                except:
                    result.addError(self, self.exc_info())
                    return
                try:
                    for test in self._tests:
                        if result.shouldStop:
                            log.debug("stopping")
                            break
                        # each nose.case.Test will create its own result proxy
                        # so the cases need the original result, to avoid proxy
                        # chains
                        test(orig)
                        
                        if getattr(test,'stopContext', None):
                            test.stopContext = None
                            break;
                finally:
                    self.has_run = True
                    try:
                        self.tearDown()
                    except KeyboardInterrupt:
                        raise
                    except:
                        result.addError(self, self.exc_info())
            setattr(suite.__class__, 'run', runWithStopSuiteOnTestFail)
            return suite
        return None

    
    def generateFromClass(self,  cls,  allowedMethods):
        tests = []
        for i in range(0, self.testsByClass):
            test = self.generateOneStress(cls, allowedMethods)
            if test:
                tests.append(test)
        return tests
        
    def wrapMethodSequence(self, cls, methodSequence):
        log.debug("wrapping method sequence %s for stress testing"
                        "from class %s",  methodSequence,  cls)
        inst = cls()
        for i in range(0, len(methodSequence)): #NOTE: the range is o.k.
            yield ChainedTestCase(method = methodSequence[i], instance = inst,  chain = methodSequence,  index = i)
        
    def prepareTest(self, test): #for THE onle root testcase
        self.rootCase = test
    
    def retry(self, test):
        setattr(test, 'stopContext',  False)
        self.rootCase.addTest(test)
        
    def storePath(self, test):
        pass
       
    def isChainedTestCase(self, test):
        if test.__class__ is ChainedTestCase:
            return True
        return False
    
    def handleFailure(self, test, err):
        testInst = getattr(test, "test", None)
        if not testInst:
            log.error("unexpected attr in handleFailure,  doesn't have test attr")
            return
        else:
            if self.isChainedTestCase(testInst):
                log.debug("catched stress test failure (%s)",  testInst)
                log.debug("chain is %s,  index %d",  testInst.chain,  testInst.index)
                setattr(test,  'stopContext',  True)
                testInst.failureBuffer.append(ZfsTestFailure(test, err))
                if len(testInst.failureBuffer) < self.retriesAfterFailure + 1:
                    self.retry(testInst)
                    return True
                else:
                    self.storePath(testInst)
                    self.reportProxy.reportFailure(testInst.failureBuffer.pop())
        return False
    
    def handleError(self, test, err):
        return self.handleFailure(test, err)
    
    def afterTest(self, test):
        testInst = getattr(test, "test", None)
        if not testInst:
            log.error("unexpected attr in handleFailure,  doesn't have test attr")
            return None
        else:
            if self.isChainedTestCase(testInst):
                if testInst.failureBuffer:
                    self.storePath(testInst)
                    self.reportProxy.reportFailure(testInst.failureBuffer.pop())
                chain = getattr(testInst, 'chain', None)
                index = getattr(testInst, 'index', None)
                if index == len(chain) - 1:
                    self.reportProxy.reportSuccess(testInst)
                return True
        return None
        

class ChainedTestCase(MethodTestCase):
    failureBuffer = []
    
    def snapshotChain(self, snapshot):
        stringChain = []
        for meth in self.chain:
            stringChain.append(str(meth))
        snapshot.addObject("stressChain", stringChain)
        snapshot.addEntry("stressChainIndex", 
                          (SnapshotDescription.TYPE_INT, self.index))
        
        if getattr(self.inst, "snapshotInstFunc", None):
            self.inst.snapshotInstFunc(snapshot)
        
    def resumeChain(self, snapshot):
        if getattr(self.inst, "resumeInstFunc", None):
            self.inst.snapshotInstFunc(self.inst, snapshot)
            
        self.index = snapshot.getEntry("stressChainIndex")
        stringChain = snapshot.getObject("stressChain")
        self.chain = []
        for methodName in stringChain:
            self.chain.append(getattr(self.inst, methodName))
        
    def generateSavedPath(self, file): #TODO: implement this
        pass
        
    def __init__(self, method, test=None, arg=tuple(), descriptor=None, instance = None,  chain = None,  index = 0):
        #NOTE: keep this in sync with __init__ of nose.case.MethodTestCase
        self.method = method
        self.test = test
        self.arg = arg
        self.descriptor = descriptor
        self.cls = method.im_class
        if instance is None:
            instance = self.cls()
        self.inst = instance
        
        # redirect 
        if not hasattr(self.inst, "snapshotInstFunc"):
            setattr(self.inst, "snapshotInstFunc", getattr(self.inst, "snapshot", None))
            setattr(self.inst, "snapshot", self.snapshotChain)
        
        if not hasattr(self.inst, "resumeInstFunc"):
            setattr(self.inst, "resumeInstFunc", getattr(self.inst, "resume", None))
            setattr(self.inst, "resume", self.resumeChain)
        
        if self.test is None:
            method_name = self.method.__name__
            self.test = getattr(self.inst, method_name)            
        self.chain = chain
        self.index = index
        TestBase.__init__(self)
        
    def shortDescription(self):
        return self.method.__name__ + " in chain %s" % self.chain
    
