""" Module with stress generator plugin for nose """

import logging
import textwrap
import unittest
import os
import sys
import tempfile
import re
import pysvn
import pickle
import nose

from optparse import OptionConflictError
from warnings import warn
from traceback import format_exc
from types import TypeType, ClassType
from random import randint

from insecticide.failure import ZfsTestFailure
from insecticide.report import ReportProxy, startTimeAttr, endTimeAttr
from insecticide.snapshot import SnapshotDescription
from insecticide.zfsReportPlugin import ZfsReportPlugin
from insecticide.graph import GraphBuilder, DependencyDeffinitionError

from nose.case import MethodTestCase,  TestBase, Test
from nose.proxy import ResultProxyFactory
from nose.plugins import Plugin
from nose.suite import LazySuite


LENGTH_INFINITE = -1
""" Define for infinite tests. This is 'virtual' length of infinite chain. """

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
       
class PruneLogic(object):
    maxIterations = 2
    """ Maximum known level where prune functions care about forbidden 
        variants. On maxIterations + 1, they could use anything.
    """
    
    def __call__(self, test, useAllMethods = False):
        """ Prune stress test chain. 
            Try to find shorter sequence from first method to last used (where index points to).
            It could use other methods that listed in chain too.
            NOTE: this method assumes that all runs share the same graph
            :Pruning methods: 
                shortest path - shortest path between chain start and chain[index] (failed method)
                disable function - random function (all occurences) in chain will be replaced with 
                    shortest path between previous and next function
                skip part - replace random part of graph with shortest path
            
            :Parameters:
                test: ChainedTestCase instance with index set to last method that should be used in chain
                useAllMethods: If use all pruning methods.
                    If False, use preferably pruning method that were not used yet.
            :Return:
                ordered list of methods - shorter path between first and indexed method. 
                or None when there is no possibility how to prune
        """
        graph = getattr(test.chain, 'graph', None)
        if not graph:
            raise DependencyDeffinitionError('No graph for test ' + str(test))
        
        forbiddenVariants = getattr(graph, 'forbiddenVariants', [])
            
        setattr(graph, 'forbiddenVariants', forbiddenVariants)
            
        # try new method
        for iteration in range(self.maxIterations + 1):
            if iteration:
                # in further iterations, we don't want to ignore blocks from 
                # previous iterations
                useAllMethods = False
            for method in [self.shortestPath, self.disableFunction, self.skipPart]:
                chain = method(graph, test.chain, test.index + 1, forbiddenVariants, iteration, useAllMethods)
                if chain and chain != test.chain[:test.index + 1]:
                    # we don't want to return the same chain
                    log.debug('method used: %s in iteration %d', method.__name__, iteration)
                    log.debug('len old: %d new %d',test.index + 1, len(chain))
                    log.debug('f:%s', str(forbiddenVariants))
                    
                    return LazyTestChain(graph, maxLength = len(chain),
                        array = chain)
                
        # nothing can be done
        return None
    
    def shortestPath(self, graph, chain, chainLength, forbiddenVariants, 
        iteration, reuseOldVariants):
        """ Try to prune chain by searching for shortest path between.
            first and failed test.
            
            :Iterations:
                 first: shortest path (just one time)
                 last: shortest path, but try if there is change in first or last test
            
            :Parameters:
                graph: graph used for generating chain
                chain: chain to prune
                chainLength: length of chain used - index of failed test + 1
                forbiddenVariants: list of key values determining 
                    method that had been used on this chain
                    format of keys is str(pruningMethod)[:own parameters]
                iteration: how many times were pruning used on this chain
                    withou success.
                    The greater is iteration, the more brutal force methods are
                    used for pruning.
                reuseOldVariants: if old variants should be reused - shortestPath ignore this
                
            :Return:
                shorter chain or None
        """
        if iteration == 0:
            # on first iteration, try only if were used
            if 'shortestPath' in forbiddenVariants:
                return None
            else:
                forbiddenVariants.append('shortestPath')
            
        elif iteration == self.maxIterations:
            # on last iteration, check if there is new start or end
            if 'shortestPath:' + chain[0] + ':' \
                    + chain[chainLength - 1] in forbiddenVariants:
                        return
                        
        else:
            # in between iterations are ignored
            return
            
        forbiddenVariants.append('shortestPath:' + chain[0] + ':' \
            + chain[chainLength - 1])
            
        path = graph.getShortestPath (chain[0], chain[chainLength - 1])
        if not path: #this should not happen - we should find at least old path
            raise Exception ("sys error: we don't find existing path")
        else:
            return path
            
            
    def skipPart(self, graph, chain, chainLength, forbiddenVariants, iteration,
        reuseOldVariants):
        """ Try to prune chain by skiping part of chain (yet preserve
            dependencies).
            
            :Iterations:
                first: try to skip random part by searching for shorter path
                    between start and end
                second: check for all function:function intervals,
                    where both functions are the same - merge them to one
                    skipping functions between them.
                last: try to skip all possible subsequences
            
            :Parameters:
                graph: graph used for generating chain
                chain: chain to prune
                chainLength: length of chain used - index of failed test + 1
                forbiddenVariants: list of key values determining 
                    method that had been used on this chain
                    format of keys is str(pruningMethod)[:own parameters]
                iteration: how many times were pruning used on this chain
                    withou success.
                    The greater is iteration, the more brutal force methods are
                    used for pruning.
                reuseOldVariants: if old variants should be reused - 
                    skip part will assume that chain is different and remove all old
                    occurences
                
            :Return:
                shorter chain or None
        """
        # we need to preserve array object (for further iterations)
        # and do backward loop not to fuzz with index
        variantIndex = len(forbiddenVariants) - 1
        while variantIndex >=0:
            if forbiddenVariants[variantIndex].startswith('skipPart'):
                forbiddenVariants.pop(variantIndex)
            variantIndex -= 1
                
        if iteration == 0:
                
            # first iteration, check for function level
            if 'skipPart' in forbiddenVariants:
                return None
            else:
                forbiddenVariants.append('skipPart')
                start = randint( 1, chainLength - 2)
                end = randint( start + 1, chainLength - 1)
                path = graph.getShortestPath(chain[start], chain[end])
                if not len(path) < end - start + 1:
                    return None
                else:
                    forbiddenVariants.append('skipPart:' + str(start) + ':' + str(end))
                    return chain[0:start] + path + chain[end + 1:chainLength]
                    
        elif iteration == 1:
            # second iteration, check for all function to function
            functionsToUse = graph.getNodeList()
            functionsToUse.remove(chain[0])
            try:
                functionsToUse.remove(chain[chainLength - 1])
            except ValueError:
                # could be the same as first
                pass
            for function in functionsToUse:
                if 'skipPart:' + function in forbiddenVariants:
                    #there is no pair of this function
                    continue
                try:
                    start = chain.index(function)
                    end = None
                except ValueError:
                    forbiddenVariants.append('skipPart:' + function)
                    continue
                try:
                    end = start + chain[start + 1:].index(function) + 1
                    if end >= chainLength:
                        #don't skip after failure
                        end = None
                except ValueError:
                    forbiddenVariants.append('skipPart:' + function)
                    continue
                    
                if start is not None and \
                    end is not None and \
                    'skipPart:' + str(start) + ':' + str(end) \
                    not in forbiddenVariants:
                    break
                
            if start is None or \
                end is None or \
                'skipPart:' + str(start) + ':' + str(end) \
                in forbiddenVariants:
                return None
            else:
                #log.debug('skip: (%d,%d) %s\n %s - %s', start, end, 
                #    str(chain[start : end]), chain[0:start], chain[end:])
                forbiddenVariants.append('skipPart:' + str(start) + ':' + str(end))
                return chain[0:start] + chain[end:chainLength]
            
        elif iteration == self.maxIterations:
            # check for all intervals
            for index in range(chainLength - 2):
                for length in range(2, 2 + chainLength - 2 - index):
                    if 'skipPart:' + str(index) + ':' + str(index + length)\
                        in forbiddenVariants:
                        continue
                    bypass = graph.getShortestPath(chain[index],
                        chain[index + length])
                    forbiddenVariants.append('skipPart:' + str(index) + ':' + str(index + length))
                    if len(bypass) <= length:
                        #log.debug('skip: (%d,%d) %s\n %s - %s - %s', index, index+length, 
                        #      chain, chain[0:index], bypass, chain[index + length + 1:])
                        return chain[0:index] + bypass + chain[index + length + 1:chainLength]
        else:
            return None
        
    def disableFunction(self, graph, chain, chainLength, forbiddenVariants, 
        iteration, reuseOldVariants):
        """ Try to prune chain by disabling one function. Any function occurence
            is skipped, if there is bypass between previous and next function.
            
            :Iterations:
                first: try to disable random function
                last: try to skip any function
            
            :Parameters:
                graph: graph used for generating chain
                chain: chain to prune
                chainLength: length of chain used - index of failed test + 1
                forbiddenVariants: list of key values determining 
                    method that had been used on this chain
                    format of keys is str(pruningMethod)[:own parameters]
                iteration: how many times were pruning used on this chain
                    withou success.
                    The greater is iteration, the more brutal force methods are
                    used for pruning.
                reuseOldVariants: if old variants should be reused -
                    disableFunction ignores this - the function are there no more
                    (NOTE: there is some possibility that skipPart could enable
                    new paths, but it will find them too)
                
            :Return:
                shorter chain or None
        """
        # by default, skip anything except first and last
        # TODO: consider only functions from path
        functionsToUse = graph.getNodeList()
        functionsToUse.remove(chain[0])
        try:
            functionsToUse.remove(chain[chainLength - 1])
        except ValueError:
            #could be the same
            pass
            
        if iteration == 0:
            #check for function level
            if 'disableFunction' in forbiddenVariants:
                # only one try on first level
                functionsToUse = []
            else:
                forbiddenVariants.append('disableFunction')
        elif iteration == self.maxIterations:
            # try all functions on last level
            functionsToUse = [function for function in functionsToUse \
                if 'disableFunction:' + function not in forbiddenVariants]
        else:
            functionsToUse = []
            
        if not functionsToUse:
            return None
            
        function = functionsToUse[0]
        forbiddenVariants.append('disableFunction:' + function)
        newChain = [chain[0]]
        for index in range(1,chainLength - 2):
            if not chain[index] == function:
                newChain.append(chain[index])
            else:
                # get bypass
                replacePath = graph.getShortestPath(newChain[len(newChain) - 1],
                    chain[index + 1], [function])
                
                if replacePath:
                    # we found bypass
                    newChain.extend(replacePath[1:len(replacePath) - 1])
                else:
                    # there is no bypass
                    newChain.append(chain[index])
                    
        newChain.extend(chain[chainLength - 2:chainLength])
            
        return newChain
        

class StressGenerator(Plugin):
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
    
    
    # savedPath related variables
    savedPathDir = None
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
    
    metaTestCollector = None
    """ MetaTestCollector class instance. Used for collecting meta methods. """
    
    pruneLogic = None
    """ Instance of pruning class """
    
    svnClient = None
    """ Pysvn svn client instance. Used for adding saved paths into subversion. """
    
    reportProxy = None
    """ Report proxy object used for reporting chain successes or failures """
    
    
    chainQueue = None
    """ List of generated chains, have to be appended to root case before run. """
    
    def __init__(self):
        Plugin.__init__(self)
        self.svnClient = pysvn.Client()
        self.metaTestCollector = MetaTestCollector()
        self.pruneLogic = PruneLogic()
        self.chainQueue = []
        self.savedPathDir = os.path.join( os.getcwd(), 'savedPaths')
        
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
            
        # config and resultProxy - we will need them to construct 
        # our own ContextSuites
        self.conf = conf
        self.resultProxy = ResultProxyFactory(config=conf)
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
        classGraph.setStopProbability(self.stopProbability)
        
        # we use LazyTestChain to allow virtually infinite chains
        methodSequence = LazyTestChain(graph = classGraph, 
            maxLength = self.maxTestLength)
        
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
                methodSequence: ordered list of meta tests (string form) in order in which they should run in chain.
                    If self.maxTestLength is LENGTH_INFINITE, it should be LazyTestChain instance.
                carryAttributes: map of attributeName : value. 
                    Attributes that will be set on every test case object to given value.
                    Used to carry snapshot buffers, failure buffers and so on.
            
            :Return:
                nose ContextSuite object (actually InfiniteChainedTestSuite 
                    is used to allow more virtual tests as one object)
        """
        log.debug("wrapping method sequence %s for stress testing"
                        "from class %s",  methodSequence,  cls)
        inst = cls()
        
        theCase = ChainedTestCase(cls = cls, instance = inst,
                                        chain = methodSequence, index = 0) 
        if carryAttributes:
            for key in carryAttributes:
                try:
                    setattr(theCase, key, carryAttributes[key])
                except KeyError:
                    pass
            
        if theCase:
            log.debug("returning tests %s", str(theCase))
            suite = InfiniteChainedTestSuite(theCase, context=cls, 
                config = self.conf, resultProxy = self.resultProxy)
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
        

        
    def shouldRetry(self, test):
        """ Check, if there should be another retry for given test.
            
            :Parameters:
                test: ChainedTestCase instance
                
            :Return
                True if test iteration count is smaller than 
                    self.retriesAfterFailure
        """
        iteration = getattr(test, 'retryIteration', 0)
        
        # we don't want to rerun tests from savedPath - we don't have dependency graph
        return self.retriesAfterFailure > iteration and not hasattr(test, 'fromSavedPath')
        
    def retry(self, test, fromFailure = True):
        """ Query test for retry.
            
            :Parameters:
                test: ChainedTestCase instance
        """
        log.debug("query %s for retry", str(test))
        #self.rerunQueue.append(test)
        if hasattr(test, 'retryIteration'):
            setattr(test, 'retryIteration', test.retryIteration + 1)
        else:
            setattr(test, 'retryIteration', 1)
        carry = {}
        for key in ['failureBuffer', 'snapshotBuffer', 'retryIteration']:
            try:
                carry[key] = getattr(test, key)
            except AttributeError:
                pass
        newChain = self.pruneLogic(test, fromFailure)
        if newChain:
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
        if isinstance(test, ChainedTestCase):
            return True
        return False
        
    @classmethod
    def getTestInstance(cls, test):
        """ Get actual TestCase instance for test.
            
            :Parameters:
                test: Test or InfiniteChainedTestSuite
                
            :Return:
                TestCase instnace
        """
        if isinstance(test, Test):
            return test.test
        elif isinstance(test, InfiniteChainedTestSuite):
            return test.test.test
        else:
            return None
        
    def handleFailure(self, test, err, error = False):
        """ Catches stress test failure. Blocks all subsequent plugins 
            from seeing it (non stress failures are ignored).
            If retries are allowed, calls retry, if not saves path.
            
            .. See: nose plugin interface
        """
        log.debug('in handle failure with %s', test)
        
        testInst = self.getTestInstance(test)
        if not testInst:
            log.error("unexpected attr in handleFailure,  doesn't have test attr")
            return
            
            
        if self.isChainedTestCase(testInst):
            log.debug("catched stress test failure (%s)",  testInst)
            log.debug('set %s on %s to True', StressGenerator.stopContextAttr, test)
            setattr(test,  StressGenerator.stopContextAttr,  True)
            log.debug('set stopContextAttr on %s to True', str(id(test)))
            log.debug("failureBuffer is %s (%d)", testInst.failureBuffer, id(testInst.failureBuffer))
            testInst.failureBuffer.append(ZfsTestFailure(testInst, err, error))
            if self.shouldRetry(testInst):
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
        testInst = self.getTestInstance(test)
        if not testInst:
            log.error("unexpected attr in handleFailure,  doesn't have test attr")
            return
            
        if self.isChainedTestCase(testInst):
            (testName, description) = self.generateDescription(testInst)
            if self.shouldReport:
                log.debug("reporting %s failure from addFailure", testInst.method.__name__)
                if error:
                    self.reportProxy.reportError(ZfsTestFailure(testInst,err, error), name = testName, description = description)
                else:
                    self.reportProxy.reportFailure(ZfsTestFailure(testInst,err, error), name = testName, description = description)
            if hasattr(testInst, 'failureBuffer'):
                # delete old data
                log.debug('cleaning failureBuffer for %s', str(testInst))
                while testInst.failureBuffer:
                    testInst.failureBuffer.pop().delete()
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
        testInst = self.getTestInstance(test)
        if not testInst:
            log.error("unexpected attr in handleFailure,  doesn't have test attr")
            return
            
        if self.isChainedTestCase(testInst):
            chain = getattr(testInst, 'chain', None)
            index = getattr(testInst, 'index', None)
            if index < len(chain) - 1: #do not report partial tests for suite
                log.debug("blocking success of %s", test)
            elif testInst.failureBuffer:
                if self.shouldRetry(testInst):
                    self.retry(testInst.failureBuffer[ \
                        len(testInst.failureBuffer) - 1].test, fromFailure = False)
                else:
                    log.debug("%s failure from addSuccess", testInst.method.__name__)
                    self.storePath(testInst)
                    failure = testInst.failureBuffer.pop()
                    (testName, description) = self.generateDescription(failure.test)
                    if self.shouldReport:
                        if failure.error:
                            self.reportProxy.reportError(failure, name = testName, description = description)
                        else:
                            self.reportProxy.reportFailure(failure, name = testName, description = description)
                    #delete old data
                    log.debug('cleaning failureBuffer for %s', str(testInst))
                    while testInst.failureBuffer:
                        testInst.failureBuffer.pop().delete()
            else:
                log.debug("%s success from addSuccess ", str(testInst))
                (testName, description) = self.generateDescription(testInst)
                if self.shouldReport:
                    self.reportProxy.reportSuccess(testInst, name = testName, description = description)
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
        if isinstance(test,ChainedTestCase):
            return ("Chain for " + test.cls.__name__,
                test.shortDescription())
        elif isinstance(test, InfiniteChainedTestSuite):
            return ("Chain for " + test.test.test.cls.__name__, 
                test.shortDescription())
        else:
            return (test.shortDescription(), test.shortDescription())
    
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
    __test__ = False
    
    __globalAttributes = ['failureBuffer', 'snapshotBuffer', 'chain', 'cls', startTimeAttr, endTimeAttr]
    """  List of attributes which shoud not be accessed locally on TestCase.
        They are stored in self.inst instead
        :Items:
            failureBuffer: Buffer of failures (insecticide.failure.ZfsTestFailure) from previous runs. 
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
        
    def __hasattr__(self, name):
        if name in ChainedTestCase.__globalAttributes:
            if self.inst:
                return self.inst.__hasattr__(name)
            else:
                raise AttributeError()
                
        return super(ChainedTestCase, self).__hasattr__(name)    
        
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

        
        
    def setIndexTo(self, index, method = None, test = None):
        """ Shift this TestCase to be for test on index 'index' in chain.
            
            :Parameters:
                index: index in chain (zero based)
                method: override method on chain[index] with method
                test: override test with test
                
            :Raise:
                IndexError: if index is out of range
        """
        if not method:
            self.method = getattr(self.cls, self.chain[index])
        else:
            self.method = method
            
        if test is None:
            method_name = self.method.__name__
            self.test = getattr(self.inst, method_name) 
        
        self.index = index
        
    def __init__(self, cls, method = None, test=None, arg=tuple(),
                descriptor=None, instance = None,  chain = None,  index = 0):
        """ Initializes whole chain.
            TODO: parameters
            
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
        
        self.setIndexTo(index, method, test)
        
        TestBase.__init__(self)
        
    def shiftToNext(self):
        """ Transform this TestCase to next test in chain.
            
            :Raise:
                IndexError: if end of chain is reached
        """
        self.setIndexTo(self.index +  1)
        
    def shortDescription(self):
        """ Return short description for current test
            
            ..See: nose.case.MethodTestCase.shortDescription
        """
        if len(self.chain) > 10:
            return self.method.__name__ + " at " + str(self.index) + " in chain " + str(self.chain[:5]) + \
                " .. " + str(self.chain[self.index - 2: self.index + 1])
        else:
            return self.method.__name__ + " in chain " + str(self.chain)
        
    
class SuiteRunner(nose.suite.ContextSuite):
    """ ContextSuite successor with slightly modified run method.
        Reason for this modification is to allow addition of new suites
        in runtime.
        It is used as wrapper containing all stress test suites.
        
        ..See: nose.suite.ContextSuite
    """
    
    __test__ = False
    
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
        
        

class LazyTestChain(object):
    """ Lazy array of stress test method names.
        Used instead of normal array to provide
        virtually infinite sequences (arrays).
    """
    __overridenAttributes = ['__overridenAttributes', '__extendedAttributes', \
        '__init__', '__expand__', '__getitem__', '__setitem__', 'array', \
        'graph', 'maxLength', '__len__', '__str__', '__repr__']
    
    def __getattribute__(self, name):
        """ Overriding getattribute method for object attributes
            redirects all except ChainedTestCase.__overridenAttributes to
            self.array (array of used items)
        """
        if name in LazyTestChain.__overridenAttributes:
            return super(LazyTestChain, self).__getattribute__(name)
        elif not hasattr(self, 'array') or self.array is None:
            raise AttributeError()
        else:
            return self.array.__getattribute__(name)
                
    __getattr__ = __getattribute__
        
    def __setattr__(self, name, value):
        """ Overriding access method for object attributes
            redirects all except ChainedTestCase.__overridenAttributes to
            self.array (array of used items).
        """
        if name in LazyTestChain.__overridenAttributes:
            return super(LazyTestChain, self).__setattr__(name, value)
        elif not self.array:
            raise AttributeError()
        else:
            return self.array.__setattr__(name, value)
    
    def __init__(self, graph, maxLength = 0, array = None):
        """ Constructor. Sets length, graph to use and first item.
            
            :Parameters:
                graph: DependencyGraph instance to generate method seqence from
                maxLength: maximum length of chain (array)
                array: first part of chain
        """
        if array:
            self.array = array
        else:
            self.array = []
            
        self.graph = graph
        self.maxLength = maxLength
        
        # we need at least the first test
        self.__expand__(0)
        
        
    def __expand__(self, key):
        """ Try to expand this chain (array) to length given by key.
            Expand will stop expanding if self.maxLength is reached or
            self.graph.next returns None.
            
            :Parameters:
                key: integer (length of array) 
                    or slice instance - key.stop is usd
            
            :Raise:
                TypeError: if other type of key is given
        """
        if isinstance(key, int):
            if key < 0:
                return
            toGenerate = key - len(self) + 2
        elif isinstance(key, slice):
            if key.stop and key.stop >= 0:
                toGenerate = key.stop - len(self) + 2
            else:
                return
        else:
            raise TypeError
            
        currentLength = len(self)
        if self.maxLength != LENGTH_INFINITE and \
            currentLength + toGenerate >= self.maxLength:
            toGenerate = self.maxLength - currentLength
        while toGenerate > 0:
            method = self.graph.next()
            if method:
                self.array.append(method)
            else:
                break;
            toGenerate -= 1
    
    def __iter__(self):
        """ Iteragor provider, redirected to self.array. """
        return self.array.__iter__()
        
    def __getitem__(self, key):
        """ Container method override, If key points behind actual array length,
            tries to expand it and redirects call to self.array. 
            """
        self.__expand__(key)
        return self.array.__getitem__(key)
        
    def __setitem__(self, key, value):
        """ Container method override, If key points behind actual array length,
            tries to expand it and redirects call to self.array. 
        """
        self.__expand__(key)
        return self.array.__setitem__(key, value)
        
    def __len__(self):
        """ Container method override, redirects call to self.array. 
        """
        return len(self.array)
    
    def __str__(self):
        """ String representation of instance - returns array.__str__. """
        return self.array.__str__()
        
    __repr__ = __str__
    
    


class InfiniteChainedTestSuite(nose.suite.ContextSuite):
    """ TestSuite for infinite test sequence.
        Most parts are used with default behavior, just there is only
        one TestCase used - should be ChainedTestCase, and
        further tests are generated by .shiftToNext method
    """
    __test__ = False
    
    def __init__(self, test, context=None, factory = None,
        config=None, resultProxy=None):
        """ Constructor override.
            
            :Parameters:
                test: ChainedTestCase with index set to begin
                context: context class (class form which chain was generated
                factory: factory that has generated this suite (if any)
                config: nose configuration object
                resultProxy: nose.proxy.ResultProxy object
                
            .. See: nose.suite.ContextSuite
        """
        
        self.context=context
        self.factory = factory
        self.config=config
        self.resultProxy=resultProxy
        self.test = Test(test, config=self.config, resultProxy=self.resultProxy)
        self.snapshotedObject = test.inst
        self.has_run = False
        
        LazySuite.__init__(self, test.chain)
        
    def run(self, result):
        """ Our implementation of suite run method.
            We need this to allow stop suite only after test failure
            (skip other in suite).
            
            .. See: nose.suite.ContextSuite
        """
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
            while True:
                from insecticide.zfsStressGenerator import StressGenerator
                if getattr(self.test, StressGenerator.stopContextAttr, None):
                    setattr(self.test, StressGenerator.stopContextAttr, None)
                    log.debug('caught stopContextAttr before test %s', str(id(self.test)))
                    break;
                if result.shouldStop:
                    log.debug("stopping")
                    break
                    
                self.test(orig)
                
                if getattr(self.test, StressGenerator.stopContextAttr, None):
                    setattr(self.test, StressGenerator.stopContextAttr, None)
                    log.debug('caught stopContextAttr after test %s', str(id(self.test)))
                    break;
                    
                try:
                    self.test.test.shiftToNext()
                except IndexError:
                    log.debug('end of world reached')
                    break;
                except AttributeError:
                    log.warning('TestCase without shiftToNext passed to InfiniteTestCase')
            
        finally:
            self.has_run = True
            try:
                self.tearDown()
            except KeyboardInterrupt:
                raise
            except:
                result.addError(self, self.exc_info())
        
        pass
        
    def shortDescription(self):
        """ Return short description for this suite
        
            .. See: nose interface
        """
        if hasattr(self.test, 'shortDescription'):
            return self.test.shortDescription()
        else:
            return self.__class__.__name__
            


class StressPluginReportRetryTest(unittest.TestCase):
    """ Tests for retry, report, etc of StressGenerator plugin. 
        Variants:
            report of: success / failure
            chain possition: middle / end
            failure in: class method / instance method
            try: first try / middle try / last try
            retry: yes/no
            type: failure / error
    """
    #comment header:
    #function | chain possition | failure in | try | retry? | report of
    class FakeReport(object):
        """ Mock object for ReportProxy """
        def __init__(self):
            self.successes = []
            self.failures = []
            self.errors = []
        def reportSuccess(self, test, duration = None, name = None, description = None):
            self.successes.append(test)
        def reportFailure(self, failure, duration = None, name = None, description = None, error = False):
            log.debug('failure reported')
            self.failures.append(failure)
        def reportError(self, failure, duration = None, name = None, description = None):
            self.errors.append(failure)
            
        def fakeStorePath(self, test):
            pass
        
    class FakeTest(object):
        """ Fake TestCase """
        metaTest = True
        definitionType = GraphBuilder.USE_FLAT
        def first(self):
            pass
        def second(self):
            pass
        def last(self):
            pass
            
    def setUp(self):
        def noPrune(test, useAllMethods = True):
            return test.chain
        log.debug('setup')
        self.plugin = StressGenerator()
        self.plugin.reportProxy = self.FakeReport()
        setattr(self.plugin, 'conf', None)
        setattr(self.plugin, 'resultProxy', None)
        setattr(self.plugin, 'pruneLogic', noPrune)
        self.plugin.shouldReport = True
        self.plugin.rootCase =  SuiteRunner([])
        self.plugin.metaTestCollector.map[self.FakeTest] = ['first', 'second', 'last']
        setattr(self.plugin, 'storePath', self.plugin.reportProxy.fakeStorePath)
        
        self.test = self.FakeTest()
        self.case = ChainedTestCase( cls = self.FakeTest, instance = self.test,
            chain = ['first', 'second', 'last'],  index = 0)
        self.suite = InfiniteChainedTestSuite(self.case)
        
    def tearDown(self):
        log.debug('teardown')
        self.test = None
        self.case = None
        self.suite = None
        self.plugin = None
        
    def testReportFailures(self):
        """ Test if handleFailure and addFailure 
            blocks and reports when retries are disabled.
        """
        self.plugin.retriesAfterFailure = 0
        # failure - middle - instance - first - no - failure
        assert self.plugin.handleFailure(Test(self.case), sys.exc_info()) == False
        
        assert self.plugin.addFailure(Test(self.case), sys.exc_info())
        assert not self.test.failureBuffer
        assert len(self.plugin.reportProxy.failures) == 1 and \
            self.plugin.reportProxy.failures[0].test == self.case
        self.plugin.reportProxy.failures.pop()
        
        # error - middle - class - first - no - error
        assert self.plugin.handleError(self.suite, sys.exc_info()) == False
        
        assert self.plugin.addError(self.suite, sys.exc_info())
        assert not self.test.failureBuffer
        assert self.plugin.reportProxy.errors
        assert self.plugin.reportProxy.errors[0].test == self.case
        
        
    def testReportSuccess(self):
        """ Test if addSuccess
            report if chain on end
        """
        # success - end - method - first - no - success
        self.case.setIndexTo(2)
        self.plugin.addSuccess(Test(self.case))
        assert self.plugin.reportProxy.successes
        assert self.plugin.reportProxy.successes[0] == self.case
        
    def testBlockPrematureSuccess(self):
        """ Test if non-finished success reports are blocked. """
        # success - middle - method - first - no - success
        assert self.plugin.addSuccess(Test(self.case))
        assert not self.plugin.reportProxy.successes 
        
        
    def testRetryFromFailure(self):
        """ Test if failed test is retried upon failure. 
            And reported in last rerun.
        """
        self.plugin.retriesAfterFailure = 1
        
        # failure - middle - instance - first - retry - failure
        assert self.plugin.handleFailure(Test(self.case), sys.exc_info())
        
        assert self.plugin.rootCase.tests
        assert self.plugin.rootCase.tests[0].test.test.failureBuffer
        assert self.plugin.rootCase.tests[0].test.test.failureBuffer[0].test == self.case
        
        retry = self.plugin.rootCase.tests.pop()
        assert not self.plugin.rootCase.tests
        assert retry.test.test.index == 0
        
        # failure - middle - instance - last - retry - failure
        assert not self.plugin.handleFailure(retry.test, sys.exc_info())
        assert not self.plugin.reportProxy.failures
        assert not self.plugin.rootCase.tests
        
        retry.test.test.setIndexTo(len(retry.test.test.chain) - 1)
        assert self.plugin.addSuccess(retry.test)
        assert len(self.plugin.reportProxy.failures) == 1
        assert not self.plugin.reportProxy.successes
        
        # error - middle - class - last - retry - error
        assert not self.plugin.handleError(retry, sys.exc_info())
        assert not self.plugin.rootCase.tests
        assert len(self.plugin.reportProxy.failures) == 1
        assert not self.plugin.reportProxy.successes
        
        # success - middle - instance - end - retry - error
        retry.test.test.failureBuffer[0].error = True
        assert self.plugin.addSuccess(retry.test)
        assert len(self.plugin.reportProxy.errors) == 1
        
    def testRetrySuiteFromFailure(self):
        """ Test if failed suite is retried upon failure. """
        self.plugin.retriesAfterFailure = 1
        # failure - middle - class - first - retry - failure
        assert self.plugin.handleFailure(self.suite, sys.exc_info())
        
        assert self.plugin.rootCase.tests
        assert self.plugin.rootCase.tests[0].test.test.failureBuffer
        assert self.plugin.rootCase.tests[0].test.test.index == 0
        assert self.plugin.rootCase.tests[0].test.test.failureBuffer[0].test == self.case
        
    def testRetryFromSuccess(self):
        """ Test if succeeded retry generates new retry """
        self.plugin.retriesAfterFailure = 3
        
        # failure - middle - method - first - retry - failure
        assert self.plugin.handleFailure(Test(self.case), sys.exc_info())
        assert not self.plugin.reportProxy.successes
        assert not self.plugin.reportProxy.failures
        
        retry = self.plugin.rootCase.tests.pop()
        assert retry.test.test.failureBuffer[0].error == False
        assert not self.plugin.rootCase.tests
        assert retry.test.test.index == 0
        
        # success - end - method - middle - retry - failure
        retry.test.test.setIndexTo(len(retry.test.test.chain) - 1)
        assert self.plugin.addSuccess(retry.test)
        
        assert not self.plugin.reportProxy.successes
        assert self.plugin.rootCase.tests
        
        
        retry = self.plugin.rootCase.tests.pop()
        assert not self.plugin.rootCase.tests
        assert retry.test.test.index == 0
        
        # error - middle - class - middle - retry - error
        assert self.plugin.handleError(retry, sys.exc_info())
        
        assert not self.plugin.reportProxy.successes
        assert not self.plugin.reportProxy.failures
        assert not self.plugin.reportProxy.errors
        
        assert self.plugin.rootCase.tests
        assert self.plugin.rootCase.tests[0].test.test.failureBuffer[1].error == True
        
    def testReportSavedPath(self):
        """ Test that savedPaths are not retried. """
        self.plugin.retriesAfterFailure = 1
        
        # failure - middle - instance - first - retry - failure from saved path
        setattr(self.case, 'fromSavedPath', True)
        assert self.plugin.handleFailure(Test(self.case), sys.exc_info()) == False
        
        assert not self.plugin.rootCase.tests
        
class TestPruner(unittest.TestCase):
    class FakeTestCase(object):
        graph = {
            'first': [('second',1), ('third', 1)],
            'second': [('first', 1)],
            'third': [('second', 1), ('fourth', 1), ('fifth',1)],
            'fourth': [('second', 1)],
            'fifth':[('fifth',1), ('sixth', 1)],
            'sixth':[('fifth', 1), ('sevnth', 1)],
            'sevnth':[('sevnth', 1), ('sixth', 1)]
            }
        startingPoint = 'first'
        def first(self):
            pass
        second = third = fourth = fifth = sixth = sevnth = first
        
        definitionType = GraphBuilder.USE_GLOBAL
        
    def setUp(self):
        self.pruneLogic = PruneLogic()
        self.chain = LazyTestChain( \
            GraphBuilder.generateDependencyGraph(self.FakeTestCase),
            20)
        self.case = ChainedTestCase(cls = self.FakeTestCase, instance = self.FakeTestCase(),
            chain = self.chain,  index = 0)
        self.case.setIndexTo(19)
    def tearDown(self):
        self.pruneLogic = None
        self.chain = None
        self.case = None
        
    def checkValidity(self, chain):
        for index in range(0,len(chain) - 1):
            successors = self.FakeTestCase.graph[chain[index]]
            next = chain[index+1]
            for suc in successors:
                if suc[0] == next:
                    break
            if next != suc[0]:
                assert False
                
        return True
        
        
    def test(self):
        iterations = 100
        log.debug('default: (%d)%s', len(self.chain), str(self.chain))
        while iterations:
            log.debug('.')
            iterations -= 1
            next = self.pruneLogic(self.case)
            #log.debug( 'next   : (%d)%s', len(next), str(next))
            if next == None:
                return
            assert self.checkValidity(next)
            assert next != self.chain
    
