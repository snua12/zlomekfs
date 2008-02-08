import logging
import textwrap
import os
import tempfile
import re
import pysvn
import pickle
import nose

from optparse import OptionConflictError
from warnings import warn
from insecticide.failure import ZfsTestFailure
from insecticide.report import ReportProxy
from insecticide.snapshot import SnapshotDescription
from traceback import format_exc
from types import TypeType, ClassType

from nose.case import MethodTestCase,  TestBase
from nose.suite import ContextSuiteFactory, ContextList
from nose.plugins import Plugin

from insecticide.graph import GraphBuilder
import pickle

log = logging.getLogger ("nose.plugins.zfsStressGenerator")

fixtureMethods = [
                               'setup', 'setUp',  'setup_method', 
                               'teardown', 'tearDown', 'teardown_method',
                               'setup_class',  'setup_all', 'setupClass', 
                               'setupAll', 'setUpClass',  'setUpAll', 
                               'teardown_class', 'teardown_all', 'teardownClass',
                               'teardownAll', 'tearDownClass', 'tearDownAll' ,
                               'snapshot', 'resume'
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
    score = 6
    
    # maxTestLength related variables
    # option string for passing max stress test length to plugin
    maxTestLengthOpt = "--stressTestLength"
    # environment variable from which default max stress test length
    # should be read
    maxTestLengthEnvOpt = "STRESS_TEST_LENGTH"
    # max length of stress test (in meta test executions)
    maxTestLength = 100
    

    # command line option for testsByClass
    testsByClassOpt = "--stressTestsByClass"
    # environment variable name  for testsByClass
    testsByClassEnvOpt = "STRESS_TESTS_BY_CLASS"
    # maximum stress test chains generated for one class
    testsByClass = 2
    
    # command line option for retriesAfterFailure
    retriesAfterFailureOpt = "--stressRetriesAfterFailure"
    # environment variable name  for retriesAfterFailure
    retriesAfterFailureEnvOpt = "STRESS_RETRIES_AFTER_FAILURE"
    # how many times we should try to prune the chain and rerun
    retriesAfterFailure = 3
    
    # command line option for commitSavedPaths
    commitSavedPathsOpt = "--commitSavedPaths"
    # environment variable name  for retriesAfterFailure
    commitSavedPathsEnvOpt = "COMMIT_SAVED_PATHS"
    # indicates if saved failed stress tests should be commited into repo
    commitSavedPaths = False
    
    
    # unconfigurable variables
    stopProbability = 0
    useShortestPath = True
    savedPathDir = os.path.join( os.getcwd(), 'savedPaths')
    savedPathSuffix = '.savedPath'
    savedPathRegex = re.compile(r'.*%s$' % savedPathSuffix)
    fromSavedPathAttr = 'fromSavedPath'

    
    # name of attribute which says if test is meta
    metaAttrName = "metaTest"
    metaTestCollector = MetaTestCollector()
    svnClient = pysvn.Client()
    reportProxy = ReportProxy()
    
    # generated chains - have to be appended to root case before run
    chainQueue = []
    # queue of pruned chains for next run
    rerunQueue = []

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
                          dest=self.maxTestLengthOpt, metavar="test_length", 
                          action="store", type="int", 
                          default=env.get(self.maxTestLengthEnvOpt),
                          help="Maximal number of meta test executions in one stress test."
                                "%s (see %s) [%s]" %
                          (self.__class__.__name__, self.__class__.__name__, self.maxTestLengthEnvOpt))
                          
        # add option for max stress test generated for class
        parser.add_option(self.testsByClassOpt,
                          dest=self.testsByClassOpt, metavar="test_count", 
                          action="store", type="int", 
                          default=env.get(self.testsByClassEnvOpt),
                          help="Number of stress tests generated from one class."
                                "%s (see %s) [%s]" %
                          (self.__class__.__name__, self.__class__.__name__, self.testsByClassEnvOpt))
        
        # add option for max pruned reruns of failed chains
        parser.add_option(self.retriesAfterFailureOpt,
                          dest=self.retriesAfterFailureOpt, metavar="retry_count", 
                          action="store", type="int", 
                          default=env.get(self.retriesAfterFailureEnvOpt),
                          help="Number of retries after chain failure."
                                "%s (see %s) [%s]" %
                          (self.__class__.__name__, self.__class__.__name__, self.testsByClassEnvOpt))
                          
        # add option for commiting failed stress tests
        parser.add_option(self.commitSavedPathsOpt,
                          dest=self.commitSavedPathsOpt, metavar="yes", 
                          action="store_true",
                          default=env.get(self.commitSavedPathsEnvOpt),
                          help="If stored paths (of failed stress tests) should be commited into repo."
                                "%s (see %s) [%s]" %
                          (self.__class__.__name__, self.__class__.__name__, self.commitSavedPathsEnvOpt))
        
    
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
            log.debug("max stress test length is set to %s", self.maxTestLength)
            
        if hasattr(options,  self.testsByClassOpt):
            self.testsByClass = getattr(options,  self.testsByClassOpt,  self.testsByClass)
            
        if hasattr(options,  self.retriesAfterFailureOpt):
            self.retriesAfterFailure = getattr(options,  self.retriesAfterFailureOpt,  self.retriesAfterFailure)
            
        if hasattr(options,  self.commitSavedPathsOpt):
            self.commitSavedPaths = getattr(options,  self.commitSavedPathsOpt,  self.commitSavedPaths)
            
        
    
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
        
    def wantFile(self, file):
        log.debug('queried for file %s', file)
        return self.savedPathRegex.match(file)
        
    def loadTestsFromFile(self, filename):
        log.debug('queried to load tests from file %s', filename)
        if not self.savedPathRegex.match(filename):
            return None
        
        methodSequence = ChainedTestCase.getMethodSequenceFromSavedPath(filename)
        if methodSequence:
            suite = self.wrapMethodSequence(methodSequence[0].im_class, methodSequence,
                                carryAttributes = {"fromSavedPath" : True})
            self.chainQueue.append(suite)
            log.debug('saved path %s loaded', str(methodSequence))
        return [False]
        
    def loadTestsFromTestClass(self,  cls):
        log.debug("generating stress tests from class %s methods", cls)
        allowedMethods = self.metaTestCollector.getClassMethods(cls)
        log.debug("allowedMethods %s?",allowedMethods)
        
        if allowedMethods:
            return self.generateFromClass(cls, allowedMethods)
        else:
            log.debug("no meta tests in class %s",  cls)
        
    
    def monkeyPatchSuite(self, suite):
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

    def generateOneStress(self, cls, allowedMethods):
        if not allowedMethods:
            return None
        
        methodNames = []
        for method in allowedMethods:
            methodNames.append(method.__name__)
        
        classGraph = GraphBuilder.generateDependencyGraph(cls, methodNames)
        methodSequence = list()
        length = 0
        next = classGraph.next(stopProbability = self.stopProbability)
        
        while length < self.maxTestLength and next:
            methodSequence.append(getattr(cls, next))
            length += 1
            next = classGraph.next(self.stopProbability)
        if methodSequence:
            suite = self.wrapMethodSequence(cls, methodSequence)
            self.chainQueue.append(suite)
        return None

    
    def generateFromClass(self,  cls,  allowedMethods):
        tests = []
        for i in range(0, self.testsByClass):
            test = self.generateOneStress(cls, allowedMethods)
            if test:
                tests.append(test)
        return tests
        
    def wrapMethodSequence(self, cls, methodSequence, carryAttributes = None):
        log.debug("wrapping method sequence %s for stress testing"
                        "from class %s",  methodSequence,  cls)
        inst = cls()
        testCases = []
        
        '''
        for i in range(0, len(methodSequence)): #NOTE: the range is o.k.
            theCase = ChainedTestCase(instance = inst, method = methodSequence[i],
                                            chain = methodSequence, index = i) 
            if carryAttributes:
                for key in carryAttributes:
                    try:
                        setattr(theCase, key, carryAttributes[key])
                    except KeyError:
                        pass
            testCases.append(theCase)
        '''
        theCase = ChainedTestCase(chain = methodSequence) 
        if carryAttributes:
            for key in carryAttributes:
                try:
                    setattr(theCase, key, carryAttributes[key])
                except KeyError:
                    pass
        
        for i in range(0, len(methodSequence)): #NOTE: the range is o.k.
            testCases.append(theCase)
        
        if testCases:
            log.debug("returning tests %s", str(testCases))
            suite = self.suiteClass(ContextList(testCases, context=cls))
            self.monkeyPatchSuite(suite)
            return suite
        return None

        
    def prepareTest(self, test): #for THE onle root testcase
        self.rootCase = test
        #for chain in self.chainQueue:
        #    self.rootCase.addTest(chain)
        wrapper = SuiteRunner(self.chainQueue)
        self.rootCase.addTest(wrapper)
        self.rootCase = wrapper
    
    def retry(self, test):
        log.debug("query %s for retry", str(test))
        #self.rerunQueue.append(test)
        carry = {}
        for key in ['failureBuffer', 'snapshotBuffer']:
            try:
                carry[key] = getattr(test, key)
            except AttributeError:
                pass
        suite = self.wrapMethodSequence(test.cls, test.chain, carryAttributes = carry)
        self.rootCase.addTest(suite)
        
    def storePath(self, test):
        log.debug("trying to store %s", str(test))
        try:
            os.mkdir(self.savedPathDir)
            self.svnClient.add(self.savedPathDir)
        except OSError: #directory exists
            log.debug("storing path for %s failed: %s", str(test), format_exc())
            pass
        except pysvn._pysvn_2_5.ClientError:
            log.debug("storing path for %s failed: %s", str(test), format_exc())
            pass # under control
        (fd, fileName) = tempfile.mkstemp(dir = self.savedPathDir,
                                            prefix = test.inst.__class__.__name__,
                                            suffix = self.savedPathSuffix)
        test.generateSavedPath(fileName)
        # write a file foo.txt
        self.svnClient.add (fileName)
        
       
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
                log.debug("failureBuffer is %s (%s)", testInst.failureBuffer, str(id(testInst.failureBuffer)))
                testInst.failureBuffer.append(ZfsTestFailure(test, err))
                if len(testInst.failureBuffer) <= self.retriesAfterFailure:
                    self.retry(testInst)
                    return True
                else:
                    if not getattr(testInst,  self.fromSavedPathAttr, None):
                        self.storePath(testInst)
        return False
    
    handleError = handleFailure
    
    def addFailure(self, test, err):
        testInst = getattr(test, "test", None)
        if not testInst:
            log.error("unexpected attr in handleFailure,  doesn't have test attr")
            return None
        else:
            if self.isChainedTestCase(testInst):
                (testName, description) = self.generateDescription(test)
                self.reportProxy.reportFailure(ZfsTestFailure(test,err), name = testName, description = description)
                return True
    
    addError = addFailure
    
    def addSuccess(self, test):
        testInst = getattr(test, "test", None)
        if not testInst:
            log.error("unexpected attr in addSuccess,  doesn't have test attr")
            return None
        else:
            if self.isChainedTestCase(testInst):
                chain = getattr(testInst, 'chain', None)
                index = getattr(testInst, 'index', None)
                if index < len(chain) - 1: #do not report partial tests for suite
                    log.debug("blocking success of %s", test)
                elif testInst.failureBuffer:
                    self.storePath(testInst)
                    self.reportProxy.reportFailure(testInst.failureBuffer.pop())
                else:
                    (testName, description) = self.generateDescription(test)
                    self.reportProxy.reportSuccess(test, name = testName, description = description)
                return True
        return None
        
    @classmethod
    def generateDescription(self, test):
        if test.test.__class__ is ChainedTestCase:
            testName = "Chain for " + test.test.__class__.__name__
            description = "Method sequence < "
            for testM in test.test.chain:
                description += testM.__name__  + " "
            description += ">"
            description = description
        return (testName, description)
    
    def finalize(self, result):
        log.debug("finalizing")
        if self.commitSavedPaths:
            try:
                self.svnClient.checkin([self.savedPathDir], 'New saved paths from batch ' + os.environ['BATCHUUID'])
            except:
                log.debug("can't commit: %s", format_exc())
        

class ChainedTestCase(MethodTestCase):
    failureBuffer = []
    
    def snapshotChain(self, snapshot):
        stringChain = []
        for meth in self.chain:
            stringChain.append(meth.__name__)
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
        
    def generateSavedPath(self, filename): #TODO: implement this
        file = open(filename, 'w')
        stringChain = [self.inst.__class__]
        for meth in self.chain:
            stringChain.append(meth.__name__)
        pickle.dump(stringChain, file)
        file.close()
        
    @classmethod
    def getMethodSequenceFromSavedPath(self, filename):
        #TODO: report error type
        try:
            file = open(filename, 'r')
            chain = pickle.load(file)
            file.close()
        except:
            return None
            
        #we expect non empty list
        if not isinstance(chain, (list, tuple)) or len(chain) == 0:
            return None
        
        #with class on first possition
        if type(chain[0]) not in (ClassType, TypeType):
            return None
            
        cls = chain[0]
        
        methods = []
        #and strings - method names on others
        for methodName in chain[1:]:
            if not isinstance(methodName, str):
                return None
            try:
                methods.append(getattr(cls, methodName))
            except AttributeError:
                return None
        return methods

        
    def __init__(self, method = None, test=None, arg=tuple(), descriptor=None, instance = None,  chain = None,  index = -1):
        #NOTE: keep this in sync with __init__ of nose.case.MethodTestCase
        self.test = test
        self.arg = arg
        self.descriptor = descriptor
        self.cls = chain[0].im_class
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
        
        self.chain = chain
        self.index = index
        if not method:
            self.method = chain[index]
        else:
            self.method = method
            
        if self.test is None:
            method_name = self.method.__name__
            self.test = getattr(self.inst, method_name)            

        TestBase.__init__(self)
        
    def runTest(self):
        if len(self.chain) > self.index - 1:
            self.index += 1
            self.method = self.chain[self.index]
            method_name = self.method.__name__
            self.test = getattr(self.inst, method_name)   
            ret = MethodTestCase.runTest(self)
        return ret
        
    def shortDescription(self):
        return self.method.__name__ + " in chain " + str(self.chain) + "[" + str(self.index) + "]"
        
        
    
#class aix(threading.Condition):
    
class SuiteRunner(nose.suite.ContextSuite):
    def __init__(self, tests):
        self.context=None
        self.factory=None
        self.config=None
        self.resultProxy=None
        log.debug(self.__class__.__name__ + ".__init__()")
        self.tests = tests
        self.rest = []
        
    def addTest(self, test):
        self.tests.append(test)
        
    def __call__(self, *arg, **kwargs):
        log.debug(self.__class__.__name__ + ".__call__()")
        self.run(*arg, **kwargs)
        
    def run(self, result):
        log.debug(self.__class__.__name__ + ".run()")
        while self.tests:
            self.tests.pop(0)(result)
    
    def shortDescription(self):
        log.debug(self.__class__.__name__ + ".shortDescription()")
        return self.__class__.__name__
    
