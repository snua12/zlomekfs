""" This module implements simple wrapper around zen-unit library to allow reporting 
    to TestResultRepository
"""

import re
import os
import textwrap
import unittest

from nose.plugins import Plugin
from nose.case import TestBase

from insecticide.util import allowCoreDumps, setCoreDumpSettings
from insecticide.snapshot import SnapshotDescription

from os import path
from subprocess import Popen, PIPE
from optparse import OptionConflictError
from warnings import warn
from unittest import TestCase

class ZenException(Exception):
    """ Exception raised upon zen-unit failure """
    pass
    


def getFileType(fileName):
    """ Get short file type as returned by linux file command.
        
        :Parameters:
            fileName: path to file (relative to cwd or absolute)
            
        :Return: 
            first part of file -b description (for example 'ELF 32-bit LSB executable')
        
        :Raise:
            Exception if execution of file fails
    """
    process = Popen(args=('file', '-b', fileName), stdout=PIPE)
    process.wait()
    if process.returncode != 0:
        raise Exception("can't execute file")
    text = process.stdout.readline()
    text = text[:text.find(',', 0)]
    return text

sharedLibraryRegex = re.compile(r'ELF.*shared object')
""" Regexp which must match 'file' output to consider file as shared library """

executableRegex = re.compile(r'ELF.*executable')
""" Regexp which must match 'file' output to consider file as elf executable """

def isSharedLibrary(fileName):
    """ Test if file is shared library
    
        :Parameters:
            fileName: path to file (relative to cwd or absolute)
            
        :Return: 
            True if file is elf shared library, False otherwise
        
        :Raise:
            Exception if execution of file fails
    """
    type = getFileType(fileName)
    if sharedLibraryRegex.match(type):
        return True
    return False

def isExecutable(fileName):
    """ Test if file is executable
    
        :Parameters:
            fileName: path to file (relative to cwd or absolute)
            
        :Return: 
            True if file is elf executable, False otherwise
        
        :Raise:
            Exception if execution of file fails
    """
    type = getFileType(fileName)
    if executableRegex.match(type):
        return True
    return False
    
def isZenType(fileName):
    """ Test if file is either elf executable or elf shared library.
    
        :Parameters:
            fileName: path to file (relative to cwd or absolute)
            
        :Return: 
            True if file is shared library of executable, False otherwise
        
        :Raise:
            Exception if execution of file fails
    """
    type = getFileType(fileName)
    if executableRegex.match(type) or sharedLibraryRegex.match(type):
        return True
    return False

class ZenPlugin(Plugin):
    __test__ = False
    """ Loader for zen-unit tests.
    
        :Configuration Options:
            There are no options except for enabling this plugins' functionality.
        
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
    name = "ZenPlugin"
    """ Name used to identify this plugin in nose
        
        .. See: nose plugin interface
    """
    
    # we can run at last
    score = 1
    """ Plugin ordering field within nose, used in descending order 
        
        .. See: nose plugin interface
    """
    
    config = None
    """ Nose config given to us upon 'configure', passed to tests. """
    
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
        
    
    def configure(self, options, conf):
        """Configure the plugin and system, based on selected options.
        
        The base plugin class sets the plugin to enabled if the enable option
        for the plugin (self.enableOpt) is true.
        """
        Plugin.configure(self,  options,  conf)
        if not self.can_configure:
            return
        
        self.config = conf
        self.conf = conf
        
        if self.enabled == False:
            return
        
    def help(self):
        """Return help for this plugin. This will be output as the help
        section of the --with-$name option that enables the plugin.
        """
        if self.__class__.__doc__:
            # doc sections are often indented; compress the spaces
            return textwrap.dedent(self.__class__.__doc__)
        return "(no help available)"
        
    def wantFile(self, file):
        """ Tests if file could contain zen tests (is elf executable or shared library)
            
            .. See: nose plugin interface
        """
        if isZenType(file):
            return True
            
    def loadTestsFromFile(self, filename):
        """ Try to load tests from binary.
            Since zen-unit doesn't provide test listing, we must run them now.
            
            .. See: nose plugin interface
        """
        if isSharedLibrary(filename):
            (prefix, file) = path.split(filename)
            args = ('zenunit')
            env = {'LD_PRELOAD':file}
            if prefix:
                env['LD_LIBRARY_PATH'] = prefix
        elif isExecutable(filename):
            prefix = None
            args = (filename)
            env = {'LD_PRELOAD':'libzenunit.so'}
        else:
            return [None]
            
        #run tests
        coreSettings = allowCoreDumps()
        
        result = Popen (args = args, env = env, stdout = PIPE, stderr = PIPE)
        result.wait()
        
        setCoreDumpSettings(coreSettings)
        
        #find dump
        coreName = os.path.join('core.' + str(result.pid))
        if result.pid and os.path.isfile(coreName):
            
            return [ZenExceptionCase(errorCode = result.returncode,
                binary = args[0], library = env['LD_PRELOAD'], ldPath = prefix,
                core = coreName, stdout = result.stdout.readlines(),
                stderr = result.stderr.readlines())]
        #create test report wrappers
        else:
            return self.parseZenOutputToTests(result.stdout.readlines(), 
                result.stderr.readlines())
                
    @classmethod
    def splitTestStatus(self, statusLine):
        """ Split zen test status line ('name<tab>STATUS(errCode)') into pieces.
            
            :Parameters:
                statusLine: zen test status line
                
            :Return:
                tuple (name, status, errCode)
        """
        tab = statusLine.find('\t', 0)
        lBrace = statusLine.find('(', tab)
        rBrace = statusLine.find(')', lBrace)
        
        name = statusLine[:tab]
        status = statusLine[tab + 1:lBrace]
        errCode = statusLine[lBrace + 1:rBrace]
        
        return (name, status, int(errCode))
    
    @classmethod
    def findErrorMessage(self, testName, stderr):
        """ Search in stderr for error mesage conected to given test.
            
            :Parameters:
                testName: function name of given test
                stderr: list of stderr lines generated by zenunit
            
            :Return:
                line generated by given failing test or None
        """
        
        proto = re.compile(r'%s:\t.*\n?' % testName)
        for line in stderr:
            if proto.match(line):
                if line[len(line) - 1] == '\n':
                    return line[:len(line) - 1]
                else:
                    return line
        
    @classmethod
    def parseZenOutputToTests(self, stdout, stderr):
        """ Parse zenunit output to tests and results
            
            :Parameters:
                stdout: list of stdout lines
                stderr: list of stderr lines
                
            :Return:
                list (or generator) of TestCases
        """
        
        firstTestIndex = 0
        delimiter = re.compile(r'\=+\n')
        # skip test outputs
        for line in stdout:
            firstTestIndex += 1
            if delimiter.match(line):
                break;
                
        # no tests
        if firstTestIndex > len(stdout):
            return [None]
        
        cases = []
        for testStatus in stdout [firstTestIndex:]:
            (name, status, err) = self.splitTestStatus(testStatus)
            if err:
                message = self.findErrorMessage(name, stderr)
            else:
                message = None
            cases.append(ZenTestCase(name = name, errorCode = err,
                message = message, config = self.config))
                
        return cases

class ZenTestCase(TestBase):
    """ Wrapper which represents one zen-test result. """
    
    __test__ = False
    def __init__(self, name, errorCode, message = None, config=None, resultProxy=None):
        """ Initialize instance with all test information (test has finishied by this time)
            
            :Parameters:
                name: function name of the test
                errorCode: return code of test
                message: error message printed by test (if hasfailed)
                config: nose based attribute
                resultProxy: nose based attribute
        """
        
        if not name:
            raise Exception('non name')
        self.name = name
        self._testMethodName = 'report'
        self.errorCode = errorCode
        self.message = message
        
        self.inst = self
        self.cls = self.__class__
        self.test = self.report
        
        self.config = config
        self.resultProxy = resultProxy
        
    
    def report(self):
        """ Wrapper test function, called as 'test'. """
        if self.errorCode != 0:
            raise AssertionError('Test '  + self.name + ' failed with retCode ' + str(self.errorCode))
        
    def __str__(self):
        return self.name
        
    __repr__ = __str__
        
    def shortDescription(self):
        if self.message:
            return self.message
        else:
            return self.name

class ZenExceptionCase(TestBase):
    """ TestCase representing zenunit exception (may be test segfault) """
    __test__ = False
    def __init__(self, errorCode, binary, library, ldPath = None, core = None,
        exception = None, stdout = None, stderr = None):
        """ Instance constructor, gets all information (error was generated by this time.
            
            :Parameters:
                errorCode: code returned by zenunit (Popen)
                binary: binary 'executed' to run tests
                    upon binary testing, this will be the binary
                    upon library testing, this will be zenunit
                library: library which was LD_PRELOADed to run tests
                    upon binary testing, this will be libzenunit.so
                    upon library testing, this will be the library
                ldPath: LD_LIBRARY_PATH (if used)
                core: core dump file name (if core dump was generated)
                exception: exception catched upon zenunit run (may be problem with system or python)
                stderr: stderr generated by zenunit call
                stdout: stdout generated by zenunit call
                
        """
        
        self.errorCode = errorCode
        self.binary = binary
        self.library = library
        self.ldPath = ldPath
        self.core = core
        self.exception = None
        self.stderr = stderr
        self.stdout = stdout
        
        self.inst = self
        self.cls = self.__class__
        
    def shortDescription(self):
        return "Zen-unit LD_PRELOAD=" + self.library + " " + self.binary
        
    def runTest(self, result):
        if self.exception:
            raise self.exception
        else:
            raise ZenException ("Zen-unit has failed with error code " + str(self.errorCode))
        
    def snapshot(self, snapshot):
        """ Since this class (it's instance) is used as test instance too, we snapshot 'test status'
            by calling this method - we append all given informatin here.
        """
        if path.basename(self.binary) == 'zenunit':
            snapshot.addFile('zenunit', '/usr/bin/zenunit')
        else:
            snapshot.addFile(path.basename(self.binary), self.binary, 
                SnapshotDescription.TYPE_ZEN_TEST)
        
        if self.library == 'libzenunit.so':
            snapshot.addFile('libzenunit.so', path.join(self.ldPath, self.library))
        else:
            snapshot.addFile(path.basename(self.library),
                path.join(self.ldPath, self.library),  
                SnapshotDescription.TYPE_ZEN_TEST)
            
        snapshot.addEntry('LD_LIBRARY_PATH', (SnapshotDescription.TYPE_STRING, self.ldPath))
        snapshot.addEntry('errorCode', (SnapshotDescription.TYPE_INT, self.errorCode))
        
        if self.core:
            snapshot.addFile(path.basename(self.binary) + '.core', self.core,  
                SnapshotDescription.TYPE_GCORE)
            
        if self.stdout:
            snapshot.addObject("stdout", self.stdout, 
                entryType = SnapshotDescription.TYPE_PICKLED_OUTPUT)
        if self.stderr:
            snapshot.addObject("stderr", self.stderr, 
                entryType = SnapshotDescription.TYPE_PICKLED_OUTPUT)
        
        
class ZenTypeCheckTest(TestCase):
    """ Unittest tests for type checking functions """
    
    def testIsExecutable(self):
        assert isExecutable(path.realpath('/bin/sh'))
        
    def testIsSharedLibrary(self):
        assert isSharedLibrary(path.realpath('/lib/ld-linux.so.2'))
        
    def testIsZenFile(self):
        assert isZenType(path.realpath('/lib/ld-linux.so.2'))
        assert isZenType(path.realpath('/bin/sh'))
        assert not isZenType('/etc/passwd')
        
class ZenPluginTest(TestCase):
    """ Unittest tests for ZenPlugin """
    
    def testStatusSplit(self):
        passStatus = 'zen_bin_pass_test\tPASS(0)'
        failStatus = 'zen_bin_fail_test\tFAIL(1)'
        
        (name, status, ret) = ZenPlugin.splitTestStatus(passStatus)
        #is number
        assert ret + 1 == 1
        assert name + '\t' + status + '(' + str(ret) + ')' == passStatus
        
        (name, status, ret) = ZenPlugin.splitTestStatus(failStatus)
        #is number
        assert ret - ret == 0
        assert name + '\t' + status + '(' + str(ret) + ')' == failStatus
    
    def testFindErrorMessage(self):
        stderr = ['zen_suck_test:\tother',
            'zen_bin_fail_test:\tthis should be printed',
            'zen_bin_fail_x_test:\tinvalid']
        testName = 'zen_bin_fail_test'
        
        message = ZenPlugin.findErrorMessage(testName, stderr)
        assert message == stderr[1]
        
    def testParseZenOutputToTests(self):
        stdout = []
        stderr = []
        cases = ZenPlugin.parseZenOutputToTests(stdout, stderr)
        assert not cases
        stdout.append('\n')
        stdout.append('==============================\n')
        stdout.append('zen_bin_pass_test\tPASS(0)\n')
        
        cases = ZenPlugin.parseZenOutputToTests(stdout, stderr)
        assert len(cases) == 1
        assert cases[0].name == 'zen_bin_pass_test'
        assert cases[0].message == None
        assert cases[0].errorCode == 0
        
    
    
    
if __name__ == '__main__':
    unittest.main(argv = ('unittest', 'ZenTypeCheckTest', 'ZenPluginTest'))

