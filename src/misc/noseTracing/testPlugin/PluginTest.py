from __future__ import generators

import os
import sys
import re
import textwrap
import logging
from random import SystemRandom

from types import ModuleType,  ClassType,  TypeType,  MethodType

from optparse import OptionConflictError
from nose.plugins import Plugin
from nose.case import MethodTestCase
from nose.suite import ContextList
from warnings import warn
from nose.suite import ContextSuite

log = logging.getLogger("nose.plugins.zfsPlugin")  

class PluginTest(Plugin):
    """ Test plugin framework
         At this moment plugin just implements some functions and
         print info on call.
    
    """
    can_configure = False
    enabled = False
    enableOpt = None
    globalGraphOpt = '--useGlobalGraph'
    globalGraph = False
    name = "PluginTest"
    score = 100
    
    terminatingPercentage = 10
    
    random = SystemRandom()
    
    

    def __init__(self):
        Plugin.__init__(self)
            
    def addOptions(self, parser, env=os.environ):
        """Add command-line options for this plugin.

        The base plugin class adds --with-$name by default, used to enable the
        plugin.
        """
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
        env_opt = 'TEST_USE_GLOBAL_GRAPH'
        env_opt = env_opt.replace('-', '_')
        
        parser.add_option(self.globalGraphOpt,
                          action="store_true",
                          dest=self.globalGraphOpt,
                          default=env.get(env_opt),
                          help="Use global dependency graph %s" %
                          (env_opt))


    def configure(self, options, conf):
        """Configure the plugin and system, based on selected options.

        The base plugin class sets the plugin to enabled if the enable option
        for the plugin (self.enableOpt) is true.
        """
        Plugin.configure(self,  options,  conf)
        log.debug("in configure")
        if not self.can_configure:
            return
        self.conf = conf
        if hasattr(options, self.enableOpt):
            log.debug("enabled opt")
            self.enabled = getattr(options, self.enableOpt)
        
        if self.enabled == False:
            log.debug("wtz disabled")
            return
        
        if hasattr(options, self.globalGraphOpt):
            log.debug("mame kontakt, MAME KONTAKT")
            self.globalGraph = getattr(options, self.globalGraphOpt)
        else:
            log.debug("%s NOT FOUND",  self.globalGraphOpt)
    
    def help(self):
        """Return help for this plugin. This will be output as the help
        section of the --with-$name option that enables the plugin.
        """
        if self.__class__.__doc__:
            # doc sections are often indented; compress the spaces
            return textwrap.dedent(self.__class__.__doc__)
        return "(no help available)"

    # Compatiblity shim
    def tolist(self, val):
        warn("Plugin.tolist is deprecated. Use nose.util.tolist instead",
             DeprecationWarning)
        return tolist(val)
    
    def loadTestsFromModule(self,  module):
        log.debug("load tests from module %s?",  module)
        if type(module) is not ModuleType:
            raise Exception ()
        classes = self.getMatchedTypes(module,
                                        (ClassType, TypeType), 
                                        self.testMatch.search)
        tests = []
        for cls in classes:
            cases = self.loadTestsFromClass(cls)
            if cases:
                tests.append (ContextSuite(tests = cases,   context = cls,  config = self.conf))
                
        return ContextSuite(tests)
    
    testMatch = re.compile(r'.*[Mm]eta[^\.]*')
    
    def getMatchedTypes(self,  obj,  types,  matchFunc):
        ret = []
        for name in dir(obj):
            if not matchFunc(name):
                continue
            item = getattr(obj,  name,  None)
            if type(item)  in types:
                log.debug("append object %s",  name)
                ret.append(item)
        
        return ret
    
    def describeTest(self,  test):
        return getattr(test.test, "__name__",  str(test))
    
    def wantClass(self,  cls):
        log.debug("want class %s ?",  cls.__name__)
        if self.testMatch.search(cls.__name__) is not None:
            return True
        return 
        
#    def wantMethod(self,  method):
#        log.debug("want method %s ?",  method.__name__)
#        if self.testMatch.search(method.__name__) is not None:
#            return True
#        return False
    
#class SortedSuite(ContextSuite):
#    __init__

    def loadTestsFromClass(self,  cls):
        if type(cls) not in [ClassType,  TypeType]:
            raise "panic"
        log.debug("load tests from class %s",  cls)
        methods = self.getMatchedTypes(cls, 
                                       [MethodType],  
                                       self.testMatch.search)
        
        if not self.globalGraph:
            return self.makeSuiteUsingMethodGraphs(methods,  cls)
        else:
            return self.makeSuiteByGraph(methods, cls)

    def makeSuiteByGraph(self, methods, cls):
        log.debug("USING GLOBAL GRAPH")
        graph = getattr(cls,  'graph',  None)
        if graph is None or not methods:
            return self.makeSuiteWithoutDependencies(methods, cls)
            
        defaultTargets = []
        for method in methods:
            defaultTargets.append((method.__name__, self.terminatingPercentage))
            
        log.debug("default %s",  defaultTargets)
        
        for method in methods:
            if method.__name__ not in graph:
                graph[ method.__name__] =  defaultTargets
        
        cases = []
        node = graph[methods[0].__name__]
        name = methods[0].__name__
        
        while node:
            method = getattr(cls, name,  None)
            cases.append (MethodTestCase(method))
            range = 0
            for i in node:
                range += i[1]
            
            seed = self.random.randrange(0, range + 1 + self.terminatingPercentage, 2)
            log.debug("range %s, seed %s",  range,  seed)
            
            if seed > range:
                node = None
                break
            
            for i in node:
                seed -= i[1]
                if seed <=0:
                    node = graph[i[0]]
                    name = i[0]
                    break
        
        return cases
    
    def makeSuiteUsingMethodGraphs(self,  methods,  cls):
        log.debug("USING LOCAL GRAPHS")
        cases = []
        node = methods[0]
        
        while node:
            log.debug("adding method %s",  node )
            cases.append (MethodTestCase(node))
            range = 0
            ancestors = getattr(node, "ancestors",  None)
            if ancestors:
                for i in node.ancestors:
                    range += node.ancestors[i]
                seed = self.random.randrange(0, range + 1 + self.terminatingPercentage, 2)
                log.debug ("range is %s, seed %s",  range,  seed)
                
                if seed > range:
                    node = None
                else:
                    for i in node.ancestors:
                        seed -= node.ancestors[i]
                        if seed <=0:
                            node = getattr(cls, i,  None)
                            break
            else:
                seed = self.random.randrange(0,  len(methods) +1,  2)
                if seed >= len(methods):
                    node = None
                else:
                    node = methods[seed]
        
        return cases
    
    def makeSuiteWithoutDependencies(self,  methods,  cls):
        methods.sort(reverse=True)
        
        for method in methods:
            yield MethodTestCase(method)
#        cases = []
#        for method in methods:
#            log.debug ("creating test case for method %s", method)
#            cases.append(MethodTestCase(method))
#        return cases
    
    def addError(self):
        return
    def addFailure(self):
        return
    def report(self,  stream):
        return
    def formatFailure(self,  test,  err):
        pass
    def formatFailure(self,  test,  err):
        pass
    def addSuccess(self, test):
        pass
    def afterTest(self,  test):
        log.debug("after test %s?",  test)
        #TODO: snapshot
        pass
    def beforeTest(self,  test):
        log.debug("before test %s?",  test)
        #TODO: snapshot
        pass
    def handleError(self, test, err):
        pass
    def handleFailure(self, test, err):
        pass
    def makeTest(self, obj, parent):
        pass
    def prepareTest(self, test): #for THE onle root testcase
        pass
    def prepareTestCase(self, test):
        pass
    def prepareTestLoader(self, loader):
        pass
    def prepareTestResult(self, result):
        pass
    def prepareTestRunner(self,  runner):
        pass
    def report(self,  stream):
        pass
    def setOutputStream(self, stream):
        pass
    def startContext(self, context):
        pass
    def startTest(self, test):
        pass
    def stopContext(self, context):
        pass
    def stopTest(self, test):
        pass
    def testName(self, test):
        pass
    
    def begin(self):
        #TODO: start zfs ;)
        log.debug("before all")
        pass
    def finalize(self,  result):
        log.debug("after all")
        #final cleanup
        pass
    def beforeImport(self,  filename, module):
        pass
    def beforeDirectory(self,  path):
        pass
        
    '''   
    # Compatiblity shim
    def tolist(self, val):
        warn("Plugin.tolist is deprecated. Use nose.util.tolist instead",
             DeprecationWarning)
        return tolist(val)
    
    def afterTest(self,  test):
        #TODO: snapshot
        pass
    def beforeTest(self,  test):
        #TODO: snapshot
        pass
    
    def begin(self):
        #TODO: start zfs ;)
        pass
    def finalize(self,  result):
        #final cleanup
        pass
    #handleError handleFailure 
    
    def loadTestsFromModule(self,  module):
      if getattr(module, "zfsModule", None) is None:
        return
      #load classes
      classSuites = [self.loadTestsFromClass(self, cls)
                     for cls in filter(self.wantClass,  (dir(cls)))]
      if classSuites is None:#empty
        return
      #create context
      moduleSetup = None
      moduleTeardown = None
      #wrap classes
      return testSuite()
    
    def loadTestsFromClass(self,  cls):
      if getattr(cls,  "zfsClass",  None) is None:
        return
      #make tests
      tests = [self.makeTest(getattr(cls, method))
        for method in filter(self.wantMethod, dir(cls))]
      if tests is None:
        return
      #sort tests
      tests.sort()
      #make suite
      return    ContextSuite(tests, cls)
    '''
    
