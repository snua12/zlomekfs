import os
import textwrap
from optparse import OptionConflictError
from traceback import format_exc
from warnings import warn
import datetime
import logging

from nose.plugins import Plugin
from insecticide.failure import ZfsTestFailure
from insecticide.report import ReportProxy, startTimeAttr, endTimeAttr

log = logging.getLogger ("nose.plugins.zfsReportPlugin")

class ZfsReportPlugin(Plugin):
    """ Report test results to repository
    """
    can_configure = False
    enabled = False
    enableOpt = None
    name = "ZfsReportPlugin"
    # to be sure to run LAST
    score = 2
    
    def __init__(self):
        Plugin.__init__(self)
        self.reporter = ReportProxy()
        
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
        
    def startTest(self, test):
        if hasattr(test, "test"): #only real tests
            if not hasattr(test.test, startTimeAttr): #prevent rewrite
                log.debug("setting time %s for test %s", str(datetime.datetime.now()), str(test.test))
                setattr(test.test, startTimeAttr, datetime.datetime.now())
        
    
    def addFailure(self, test, err, error = False):
        if hasattr(test, "test"): #ignore context suites
            try:
                if error:
                    self.reporter.reportError(ZfsTestFailure(test, err))
                else:
                    self.reporter.reportFailure(ZfsTestFailure(test, err))
            except:
                log.debug("exception when reporting failure:\n%s", format_exc())
                pass #TODO: specific exception and log
    
    def addError(self, test, err):
        return self.addFailure(test = test, err = err, error = True)
    
    def addSuccess(self, test):
        try:
            log.debug("reporting success of %s", test)
            self.reporter.reportSuccess(test)
        except:
            log.debug("exception when reporting success:\n%s", format_exc())
            pass #TODO: specific exception and log
    
    def finalize(self, result):
        self.reporter.finalize()

