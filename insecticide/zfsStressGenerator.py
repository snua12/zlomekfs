""" Module with stress generator plugin for nose """

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
from insecticide.zfsReportPlugin import ZfsReportPlugin
from traceback import format_exc
from types import TypeType, ClassType

from nose.case import MethodTestCase,  TestBase
from nose.suite import ContextSuiteFactory, ContextList
from nose.plugins import Plugin

from insecticide.graph import GraphBuilder

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
""" method names that are considered as non-tests by default """

class MetaTestCollector(object):
    """ Class which collects meta tests from classes and returns them as list of attribute names. """
    map = {}
    """ map of class : methodList pairs"""
    #NOTE: we rely on attrib plugin assuming that no disabled test method goes to stress plugin 
    #   and on stress plugin to give us meta methods before asking for collection.
    #   Since this is valid and saved paths are loaded with method list, it works well.
    def isMetaClass (self,  cls):
        """ Tests if class is meta class (should contain meta tests
            
            :Parameters:
                cls: class object to check
            :Return:
                True if is metaclass, False otherwise
        """
        return getattr(cls, StressGenerator.metaAttrName,  False)
    
    def add(self,  method):
        """ Adds method to list of meta tests for class
        
        :Parameters:
            method: method object
        """
        classRow = self.map.get(method.im_class,  [])
        classRow.append(method.__name__)
        self.map[method.im_class] = classRow
    
    def getClassMethods(self,  cls):
        """ Returns methods recognized as meta for given class
        
        :Parameters:
            cls: class object
        :Return:
            list of attribute names (strings) which are recognized as meta methods for given class
        """
        return self.map.get(cls,  None)

class StressGenerator(Plugin):
#FIXME: be robust against invalid meta classes (and report failures)
    """ ZFS stress test generator. 
        Plugin for nose. If enabled, tries to find meta tests inside classes 
        and generate chains from them. If there are some failed chains 
        from past, plugin tries to load them too.
        
        :Configuration Options:
            stressTestsByClass: how many stress tests will be generated from one class
            stressTestLength: maximum length of stress test (how many metatest executions)
            stressRetriesAfterFailure: how many times try to prune test after failure
            commitSavedPaths: if saved path is generated, plugin adds it to subversion. 
                When this option is enabled, it commits all saved paths after batch finalization.
            stopProbability: probability of terminating stress test after any metatest 
                (in addition to dependencyGraph) 0.1 means 10% to stop
                
            All options have both environment and parameter configuration option,
            parameter is always stronger.
            
        .. See: nose plugin interface
    """
    # enable related variables
    can_configure = False
    """ If configuration options should be used 
        (mainly used for blocking if there is option collision) 
        
        .. See: nose plugin interface
    """
    enabled = False
    """ If this plugin is enabled (should be False by default) 
        
        .. See: nose plugin interface
    """
    
    enableOpt = None
    """ Option name for enabling this plugin, if None, default will be used 
        
        .. See: nose plugin interface
    """
    
    # plugin name
    name = "ZfsStressGenerator"
    """ Name used to identify this plugin in nose
        
        .. See: nose plugin interface
    """
    
    # to be sure to run AFTER attrib plugin
    score = 6
    """ Plugin ordering field within nose, used in descending order 
        
        .. See: nose plugin interface
    """
    
    shouldReport = False
    """ If we should report successes or failures """
    
    # maxTestLength related variables
    maxTestLengthOpt = "--stressTestLength"
    """ option string for passing max stress test length to plugin """
    
    maxTestLengthEnvOpt = "STRESS_TEST_LENGTH"
    """  Environment variable from which default max stress test length
        should be read.
    """
    maxTestLength = 100
    """ Max length of stress test (in meta test executions) """
    
    # testsByClass related variables
    testsByClassOpt = "--stressTestsByClass"
    """ Command line option for testsByClass """
    
    testsByClassEnvOpt = "STRESS_TESTS_BY_CLASS"
    """ Environment variable name  for testsByClass """
    
    testsByClass = 2
    """ Maximum stress test chains generated for one class"""
    
    # retriesAfterFailure related variables
    retriesAfterFailureOpt = "--stressRetriesAfterFailure"
    """ Command line option for retriesAfterFailure """
    
    retriesAfterFailureEnvOpt = "STRESS_RETRIES_AFTER_FAILURE"
    """ Environment variable name  for retriesAfterFailure"""
    
    retriesAfterFailure = 3
    """ How many times we should try to prune the chain and rerun """
    
    # commitSavedPaths related variables
    commitSavedPathsOpt = "--commitSavedPaths"
    """ Command line option for commitSavedPaths """
    
    commitSavedPathsEnvOpt = "COMMIT_SAVED_PATHS"
    """ Environment variable name  for retriesAfterFailure """
    
    commitSavedPaths = False
    """ Indicates if saved failed stress tests should be commited into repo """
    
    
    # unconfigurable variables
    stopProbability = 0
    """ Probability of terminating chain after test. 0.1 means 10% """
    
    useShortestPath = True
    """ If use shortest path algorithm when prunning failed stress chains """
    
    # savedPath related variables
    savedPathDir = os.path.join( os.getcwd(), 'savedPaths')
    """  Directory where to put saved paths for failed chains. """
    
    savedPathSuffix = '.savedPath'
    """ Filename suffix for saved path files. """
    
    savedPathRegex = re.compile(r'.*%s$' % savedPathSuffix)
    """ Regex which must match for filename to be considered as saved path. """
    
    fromSavedPathAttr = 'fromSavedPath'
    """ Attribute name which is set for chain if chain was loaded from saved path """
    
    stopContextAttr = 'stopContext'
    """ Name of attribute that should be checked for on test case object. If set to true,
        suite should stop and skip remaining cases.
    """
    
    metaAttrName = "metaTest"
    """ Name of attribute which says if class (method) is meta """
    
    metaTestCollector = MetaTestCollector()
    """ MetaTestCollector class instance. Used for collecting meta methods. """
    
    svnClient = pysvn.Client()
    """ Pysvn svn client instance. Used for adding saved paths into subversion. """
    
    reportProxy = None
    """ Report proxy object used for reporting chain successes or failures """
    
    
    chainQueue = []
    """ List of generated chains, have to be appended to root case before run. """
    
    def __init__(self):
        Plugin.__init__(self)
    
    def addOptions(self, parser, env=os.environ):
        # for backward conpatibility
        self.add_options(parser, env)
        
    def add_options(self, parser, env=os.environ):
        # Non-camel-case version of func name for backwards compatibility.
        try:
            self.options(parser, env)
            self.can_configure = True
        except OptionConflictError, e:
            warn("Plugin %s has conflicting option string: %s and will "
                 "be disabled" % (self, e), RuntimeWarning)
            self.enabled = False
            self.can_configure = False
            
    def options(self, parser, env=os.environ):
        """ Adds options for this plugin: maxTestLength, testsByClass,
            retriesAfterFailure, commitSavedPaths.
            
            .. See: nose plugin interface
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
        """ Checks options for this plugin: enableOpt, maxTestLength, 
            testsByClass, retriesAfterFailure, commitSavedPaths 
            and configure plugin according them.
            
            .. See: nose plugin interface
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
            
        # hack: try to find out if we should report
        if hasattr(options,  ZfsReportPlugin.enableOpt):
            self.shouldReport = getattr(options,  ZfsReportPlugin.enableOpt,  self.shouldReport)
            log.debug("stress test generator will report: %s", self.shouldReport)
            
        if self.shouldReport:
            self.reportProxy = ReportProxy()
        
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
        
        .. See: nose plugin interface
        """
        if self.__class__.__doc__:
            # doc sections are often indented; compress the spaces
            return textwrap.dedent(self.__class__.__doc__)
        return "(no help available)"
    
    def wantMethod(self,  method):
        """ Returns False if method is meta to block
            running of meta tests as normal.
            
            .. See: nose plugin interface
        """
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
        """ Tests if files are saved paths and returns true if so. 
            
            .. See: nose plugin interface
        """
        log.debug('queried for file %s', file)
        return self.savedPathRegex.match(file)
        
    def loadTestsFromFile(self, filename):
        """ Try to load chain from saved path file.
            Store them into queue, they should be appended at the end.
            
            .. See: nose plugin interface
        """
        
        # ignore non saved paths
        log.debug('queried to load tests from file %s', filename)
        if not self.savedPathRegex.match(filename):
            return None
        
        # load saved path from file
        (cls, methodSequence) = ChainedTestCase.getMethodSequenceFromSavedPath(filename)
        if methodSequence:
            suite = self.wrapMethodSequence(cls, methodSequence,
                                carryAttributes = {"fromSavedPath" : True})
            self.chainQueue.append(suite)
            log.debug('saved path %s loaded', str(methodSequence))
        return [False]
        
    def loadTestsFromTestClass(self,  cls):
        """ Yields chains from test class if possible. 
            They will be stored in queue for append just before main run starts.
            
            .. See: nose plugin interface
        """
        log.debug("generating stress tests from class %s methods", cls)
        allowedMethods = self.metaTestCollector.getClassMethods(cls)
        log.debug("allowedMethods %s?",allowedMethods)
        
        if allowedMethods:
            return self.generateFromClass(cls, allowedMethods)
        else:
            log.debug("no meta tests in class %s",  cls)
        
    
    def monkeyPatchSuite(self, suite):
        """ Patches suite object (its class) switching 'run' method to stop when
            stopContext flag on test is found. This is needed for stopping chain after
            failure and not running remaining meta tests.
            
            :Parameters:
                suite: nose context suite object
        """
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
                    from insecticide.zfsStressGenerator import StressGenerator
                    if getattr(test, StressGenerator.stopContextAttr, None):
                        setattr(test, StressGenerator.stopContextAttr, None)
                        log.debug('caught stopContextAttr before test %s', str(id(test)))
                        break;
                    if result.shouldStop:
                        log.debug("stopping")
                        break
                    # each nose.case.Test will create its own result proxy
                    # so the cases need the original result, to avoid proxy
                    # chains
                    
                    test(orig)
                    
                    if getattr(test, StressGenerator.stopContextAttr, None):
                        setattr(test, StressGenerator.stopContextAttr, None)
                        log.debug('caught stopContextAttr after test %s', str(id(test)))
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
        """ Generates stress test suite from test class and list of allowed methods.
            
            :Parameters:
                cls: class object for which stress chain should be created
                allowedMethods: list of names of methods (string form)
                    which should be considered as meta on class
            
            :Return:
                None. Chain suite will be appended to self.chainQueue.
        """
        if not allowedMethods:
            return None
        
        
        classGraph = GraphBuilder.generateDependencyGraph(cls, allowedMethods)
        methodSequence = list()
        length = 0
        next = classGraph.next(stopProbability = self.stopProbability)
        
        while length < self.maxTestLength and next:
            methodSequence.append(next)
            length += 1
            next = classGraph.next(self.stopProbability)
        if methodSequence:
            suite = self.wrapMethodSequence(cls, methodSequence)
            self.chainQueue.append(suite)
        return None

    
    def generateFromClass(self,  cls,  allowedMethods):
        """ Generate self.testsByClass chains for given class
            
            :Parameters:
                cls: class object for which chains should be created
                allowedMethods: method list (in string form) which to consider as meta
                
            :Return: 
                None. Suites will be appended to self.chainQueue.
        """
        tests = []
        for i in range(0, self.testsByClass):
            test = self.generateOneStress(cls, allowedMethods)
            if test:
                tests.append(test)
        return tests
        
    def wrapMethodSequence(self, cls, methodSequence, carryAttributes = None):
        """
            :Parameters:
                cls: class for which suite (chain) should be created
                methodSequence: ordered list of meta tests (string form) in order in which they should run in chan.
                carryAttributes: map of attributeName : value. 
                    Attributes that will be set on every test case object to given value.
                    Used to carry snapshot buffers, failure buffers and so on.
            
            :Return:
                nose context suite object
        """
        log.debug("wrapping method sequence %s for stress testing"
                        "from class %s",  methodSequence,  cls)
        inst = cls()
        testCases = []
        
        for i in range(0, len(methodSequence)): #NOTE: the range is o.k.
            theCase = ChainedTestCase(cls = cls, instance = inst,
                                            chain = methodSequence, index = i) 
            if carryAttributes:
                for key in carryAttributes:
                    try:
                        setattr(theCase, key, carryAttributes[key])
                    except KeyError:
                        pass
            testCases.append(theCase)
            
        # this was code that has created one case for whole chain
        '''
        theCase = ChainedTestCase(cls = cls, chain = methodSequence) 
        if carryAttributes:
            for key in carryAttributes:
                try:
                    setattr(theCase, key, carryAttributes[key])
                except KeyError:
                    pass
        
        for i in range(0, len(methodSequence)): #NOTE: the range is o.k.
            testCases.append(theCase)
        '''
        
        if testCases:
            log.debug("returning tests %s", str(testCases))
            suite = self.suiteClass(ContextList(testCases, context=cls))
            self.monkeyPatchSuite(suite)
            return suite
        return None

    # called once for THE one root testcase
    def prepareTest(self, test):  
        """ Apends chained test cases from classes or saved paths
            into queue. Wrap them into special suite for better handling
            
            .. See: nose plugin interface
        """
        self.rootCase = test
        #for chain in self.chainQueue:
        #    self.rootCase.addTest(chain)
        wrapper = SuiteRunner(self.chainQueue)
        self.rootCase.addTest(wrapper)
        self.rootCase = wrapper
    
    def prune(self, test):
        """ Prune stress test chain. 
            Try to find shorter sequence from first method to last used (where index points to).
            It could use other methods that listed in chain too.
            The only pruning method used now is shortest path. If disabled, returns chain itself.
            
            :Parameters:
                test: ChainedTestCase instance with index set to last method that should be used in chain
            :Return:
                ordered list of methods - shorter path between first and indexed method. 
        """
        if self.useShortestPath:
            allowedMethods = self.metaTestCollector.getClassMethods(test.cls)
            log.debug("allowedMethods %s?",allowedMethods)
            graph = GraphBuilder.generateDependencyGraph(test.cls, allowedMethods) #maybe use only chain methods
            log.debug("new graph is %s", str(graph.graph))
            log.debug("searching shortest path from %s to %s", test.chain[0], test.chain[test.index])
            path = graph.getShortestPath (test.chain[0], test.chain[test.index])
            if not path: #this should not happen - we should find at least old path
                raise Exception ("sys error: we don't find existing path")
            return path
        
        return test.chain[:test.index + 1]
    
    def retry(self, test):
        """ Query test for retry.
            
            :Parameters:
                test: ChainedTestCase instance
        """
        log.debug("query %s for retry", str(test))
        #self.rerunQueue.append(test)
        carry = {}
        for key in ['failureBuffer', 'snapshotBuffer']:
            try:
                carry[key] = getattr(test, key)
            except AttributeError:
                pass
        newChain = self.prune(test)
        suite = self.wrapMethodSequence(test.cls, newChain, carryAttributes = carry)
        self.rootCase.addTest(suite)
        
    def storePath(self, test):
        """ Store path of failed stress test. Test will be stored to file
            using it's method generateSavedPath.
            Saved path file will be added to subversion (if possible).
            
            :Parameters:
                test: ChainedTestCase instance
        """
        
        log.debug("trying to store %s", str(test))
        try:
            os.mkdir(self.savedPathDir)
            self.svnClient.add(self.savedPathDir)
        except OSError: #directory exists
            log.debug("storing path for %s failed: %s", str(test), format_exc())
            pass
        except pysvn._pysvn_2_5.ClientError:
            log.debug("creating stored path dir for %s failed: %s", str(test), format_exc())
            pass # under control
        (fd, fileName) = tempfile.mkstemp(dir = self.savedPathDir,
                                            prefix = test.inst.__class__.__name__,
                                            suffix = self.savedPathSuffix)
        test.generateSavedPath(fileName)
        # write a file foo.txt
        try:
            self.svnClient.add (fileName)
        except pysvn._pysvn_2_5.ClientError:
            log.debug("storing failed due to svn error: %s", format_exc())
            pass
        
       
    def isChainedTestCase(self, test):
        """ Checks if test is ChainedTestCase
        
            :Parameters:
                test: TestCase instance
                
            :Return:
                True if instance of ChainedTestCase, False otherwise
        """
        if test.__class__ is ChainedTestCase:
            return True
        return False
    
    def handleFailure(self, test, err, error = False):
        """ Catches stress test failure. Blocks all subsequent plugins 
            from seeing it (non stress failures are ignored).
            If retries are allowed, calls retry, if not saves path.
            
            .. See: nose plugin interface
        """
        testInst = getattr(test, "test", None)
        if not testInst:
            log.error("unexpected attr in handleFailure,  doesn't have test attr")
            return
        else:
            if self.isChainedTestCase(testInst):
                log.debug("catched stress test failure (%s)",  testInst)
                #log.debug("chain is %s,  index %d",  testInst.chain,  testInst.index)
                setattr(test,  StressGenerator.stopContextAttr,  True)
                log.debug('set stopContextAttr on %s to True', str(id(test)))
                log.debug("failureBuffer is %s (%s)", testInst.failureBuffer, str(id(testInst.failureBuffer)))
                testInst.failureBuffer.append(ZfsTestFailure(test, err))
                if len(testInst.failureBuffer) <= self.retriesAfterFailure:
                    self.retry(testInst)
                    return True
                else:
                    if not getattr(testInst,  self.fromSavedPathAttr, None):
                        self.storePath(testInst)
        return False
    
    def handleError (self, test, err):
        """ Calls handleFailure with flag that this is error.
            
            .. See: nose plugin interface
        """
        return self.handleFailure (test, err, error = True)
    
    def addFailure(self, test, err, error = False):
        """ Reports stress test failure. Blocks subsequent plugins from seeing it.
            
            .. See: nose plugin interface
        """
        testInst = getattr(test, "test", None)
        if not testInst:
            log.error("unexpected attr in handleFailure,  doesn't have test attr")
            return None
        else:
            if self.isChainedTestCase(testInst):
                (testName, description) = self.generateDescription(test)
                if self.shouldReport:
                    log.debug("reporting %s failure from addFailure", testInst.method.__name__)
                    if error:
                        self.reportProxy.reportError(ZfsTestFailure(test,err), name = testName, description = description)
                    else:
                        self.reportProxy.reportFailure(ZfsTestFailure(test,err), name = testName, description = description)
                return True
    
    def addError(self, test, err):
        """ Calls addFailure with flag that this is error. 
            
            .. See: nose plugin interface
        """
        return self.addFailure(test, err, error = True)
    
    def addSuccess(self, test):
        """ Reports stress test success if this is first pass.
            If subsequent call of failed test is detected, reports failure from last run instead.
            Blocks subsequent plugins from seeing stress success (other tests are ignored).
            
            .. See: nose plugin interface
        """
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
                    failure = testInst.failureBuffer.pop()
                    (testName, description) = self.generateDescription(failure.test)
                    if self.shouldReport:
                        log.debug("reporting %s failure from addSuccess", testInst.method.__name__)
                        self.reportProxy.reportFailure(failure, name = testName, description = description)
                else:
                    (testName, description) = self.generateDescription(test)
                    if self.shouldReport:
                        self.reportProxy.reportSuccess(test, name = testName, description = description)
                return True
        return None
        
    @classmethod
    def generateDescription(self, test):
        """ Generate description for ChainedTestCase.
            Since chain is not described sufficiently by it's name, we should
            generate special strings for testName and description for TestResultStorage.
            
            :Parameters:
                test: ChainedTestCase instance
            
            :Return:
                tuple (testName, description) strings. First is short test name, second longer description.
        """
        if test.test.__class__ is ChainedTestCase:
            testName = "Chain for " + test.test.cls.__name__
            description = "Method " + test.test.chain[test.test.index] + " at " + str(test.test.index) + " in sequence: " + str(test.test.chain) # TODO: truncate
        return (testName, description)
    
    def finalize(self, result):
        """ Commits saved paths (if any) if enabled into repository
            
            .. See: nose plugin interface
        """
        log.debug("finalizing")
        if self.commitSavedPaths:
            try:
                self.svnClient.checkin([self.savedPathDir], 'New saved paths from batch ' + os.environ['BATCHUUID'])
            except:
                log.debug("can't commit: %s", format_exc())
        if self.reportProxy:
            self.reportProxy.finalize()
        

class ChainedTestCase(MethodTestCase):
    """ Wrapper class for chained cases. One instance serve
        for all tests in chain, current possition is given by index.
    """
    inst = None
    """ TestClass instance. Holds tests state and global chain state. """
    
    __globalAttributes = ['failureBuffer', 'snapshotBuffer', 'chain', 'cls']
    """  List of attributes which shoud not be accessed locally on TestCase.
        They are stored in self.inst instead
        :Items:
            failureBuffer: Buffer of failures (insecticide.failure.ZfsFailure) from previous runs. 
            snapshotBuffer: buffer of chain snapshots
            chain: ordered list of names of methods which should be called in chain
            cls: class from which chan was generated
    """
    
    def __getattribute__(self, name):
        """ Overriding getattribute method for object attributes
            redirects ChainedTestCase.__globalAttributes to self.inst
        """
        if name in ChainedTestCase.__globalAttributes:
            if self.inst:
                return self.inst.__getattribute__(name)
            else:
                raise AttributeError()
                
        return super(ChainedTestCase, self).__getattribute__(name)
        
    def __setattr__(self, name, value):
        """ Overriding access method for object attributes
            redirects ChainedTestCase.__globalAttributes to self.inst
        """
        if name in ChainedTestCase.__globalAttributes:
            if self.inst:
                self.inst.__setattr__(name, value)
            else:
                raise AttributeError()
                
        super(ChainedTestCase, self).__setattr__(name, value)
    
    def snapshotChain(self, snapshot):
        """ Monkeypatch function for snapshotting test in chain.
            Snapshots chain related attributes and calls test snapshot 
            function afterwards (if provided).
            
            :Parameters:
                snapshot: insecticide.snapshot.SnapshotDescription 
                    instance to store information into
        """
        
        snapshot.addObject("testClass", self.cls)
        snapshot.addObject("stressChain", self.chain)
        snapshot.addEntry("stressChainIndex", 
                          (SnapshotDescription.TYPE_INT, self.index))
        
        if getattr(self.inst, "snapshotInstFunc", None):
            self.inst.snapshotInstFunc(snapshot)
        
    def resumeChain(self, snapshot):
        """ Monkeypatch function for resuming test in chain.
            Calls test resume function (if provided) and
            resume chain related state afterwards.
            
            :Parameters:
                snapshot: insecticide.snapshot.SnapshotDescription 
                    instance to load state from
        """
        
        if getattr(self.inst, "resumeInstFunc", None):
            self.inst.snapshotInstFunc(self.inst, snapshot)
            
        self.cls = snapshot.getObject("testClass")
        self.index = snapshot.getEntry("stressChainIndex")
        self.chain = snapshot.getObject("stressChain")
        self.method = getattr(self.cls, self.chain[self.index])
        self.test = self.method
        
    def generateSavedPath(self, filename):
        """ Saves test sequence described by this case into file.
            Currently pickling of list is used, first item is meta test class object,
            following items are ordered method names.
            
            :Parameters:
                filename: name of file to write saved path description into
        """
        file = open(filename, 'w')
        stringChain = [self.inst.__class__]
        stringChain.extend(self.chain)
        pickle.dump(stringChain, file)
        file.close()
        
    @classmethod
    def getMethodSequenceFromSavedPath(self, filename):
        """ Retrieves test sequence (saved path) from file
            Assumes that file format is: pickled list, first item is meta test class object,
            following items are ordered method names.
            
            :Parameters:
                filename: name of file to load saved path description from
                
            :Return:
                tuple (class, methods). Where class is class object and 
                    methods is sequence of method names (string form)
                or None when nothing can be loaded
        """
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
        
        methods = chain[1:]
        return (cls,methods)

        
    def __init__(self, cls, method = None, test=None, arg=tuple(),
                descriptor=None, instance = None,  chain = None,  index = 0):
        """ Initializes whole chain.
            
            ..See: nose.case.MethodTestCase.__init__
        """
        #NOTE: keep this in sync with __init__ of nose.case.MethodTestCase
        #NOTE: self.inst should be set first, because some other attributes are redirected do self.inst.<attr>
        if instance is None:
            instance = cls()
        self.inst = instance
        self.cls = cls
        setattr(self, 'failureBuffer', [])
        setattr(self, 'snapshotBuffer', [])
        self.test = test
        self.arg = arg
        self.descriptor = descriptor
        
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
            self.method = getattr(self.cls, chain[index])
        else:
            self.method = method
            
        if self.test is None:
            method_name = self.method.__name__
            self.test = getattr(self.inst, method_name)            

        TestBase.__init__(self)
        
    def shortDescription(self):
        """ Return short description for current test
            
            ..See: nose.case.MethodTestCase.shortDescription
        """
        # FIXME: return correct test before run, in run and after run :/
        if len(self.chain) > 10:
            return self.method.__name__ + " in chain " + str(self.chain[:5]) + \
                " .. " + str(self.chain[self.index - 2: self.index + 1])  + "[" + str(self.index) + "]"
        else:
            return self.method.__name__ + " in chain " + str(self.chain) + "[" + str(self.index) + "]"
        
    
class SuiteRunner(nose.suite.ContextSuite):
    """ ContextSuite successor with slightly modified run method.
        Reason for this modification is to allow addition of new suites
        in runtime.
        It is used as wrapper containing all stress test suites.
        
        ..See: nose.suite.ContextSuite
    """
    
    def __init__(self, tests):
        self.context=None
        self.factory=None
        self.config=None
        self.resultProxy=None
        self.tests = tests
        
    def addTest(self, test):
        self.tests.append(test)
        
    def __call__(self, *arg, **kwargs):
        self.run(*arg, **kwargs)
        
    def run(self, result):
        while self.tests:
            self.tests.pop(0)(result)
    
    def shortDescription(self):
        return self.__class__.__name__
    
