""" Module containing nose plugin responsible for reporting test results. """
import os
import sys
import textwrap
from optparse import OptionConflictError
from traceback import format_exc
from warnings import warn
import datetime
import logging

from nose.plugins import Plugin
from insecticide.failure import ZfsTestFailure
from insecticide.report import ReportProxy, startTimeAttr

log = logging.getLogger ("nose.plugins.zfsReportPlugin")

class ZfsReportPlugin(Plugin):
    """ Plugin responsible for test result reporting for zfs projects.
        Currently reporting into TestResultStorage using report.ReportProxy
        
        :Configuration Options:
            None
            ReportProxy is configured by environment variables listed in its doc.
        
        .. See nose plugin interface
        .. See insecticide.report.ReportProxy
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
    
    name = "ZfsReportPlugin"
    """ Name used to identify this plugin in nose
        
        .. See: nose plugin interface
    """
    
    # to be sure to run LAST (but leave small gap for future)
    score = 2
    """ Plugin ordering field within nose, used in descending order 
        
        .. See: nose plugin interface
    """
    
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
        """ Adds options for this plugin: (currently only enable opt).
            
            .. See: nose plugin interface
        """
        Plugin.options(self,  parser,  env)
        
    
    def configure(self, options, conf):
        """ Checks options for this plugin: (only enable opt)
            and then initialize ReportProxy
            
            .. See: nose plugin interface
        """
        Plugin.configure(self,  options,  conf)
        if not self.can_configure:
            return
        
        if self.enabled == False:
            return
        
        self.reporter = ReportProxy()
    
    def help(self):
        """Return help for this plugin. This will be output as the help
        section of the --with-$name option that enables the plugin.
        
            .. See: nose plugin interface
        """
        if self.__class__.__doc__:
            # doc sections are often indented; compress the spaces
            return textwrap.dedent(self.__class__.__doc__)
        return "(no help available)"
        
    def startTest(self, test):
        """ Set start time for test.
            
            .. See: nose plugin interface
        """
        if hasattr(test, "test"): #only real tests
            if not hasattr(test.test, startTimeAttr): #prevent rewrite
                log.debug("setting time %s for test %s", str(datetime.datetime.now()), str(test.test))
                setattr(test.test, startTimeAttr, datetime.datetime.now())
        
    
    def addFailure(self, test, err, error = False):
        """ Report failures and errors
            
            :Parameters:
                test: from nose plugin interface
                err: from nose plugin interface
                error: boolean indicating if reported item is 'failure' (False) or 'error' (True)
                
            :Return:
                None - don't block successive plugins
                
            .. See: nose plugin interface
        """
        if hasattr(test, "test"): #ignore context suites
            try:
                log.debug("reporting failure of %s", test)
                if error:
                    self.reporter.reportError(ZfsTestFailure(test, err))
                else:
                    self.reporter.reportFailure(ZfsTestFailure(test, err))
            except:
                info = sys.exc_info()
                try:
                    self.reporter.reportSystemError(name = "Exception in zfsReportPlugin",
                        description = "Unexpected exception in ZfsReportPlugin.addFailure",
                        errInfo = info)
                except:
                    log.debug("exception when reporting failure:\n%s", format_exc())
                    
    
    def addError(self, test, err):
        """ Report test error. Redirects to addFailure with error = True
            
            .. See: nose plugin interface
        """
        return self.addFailure(test = test, err = err, error = True)
    
    def addSuccess(self, test):
        """ Report test success.
            
            .. See: nose plugin interface
        """
        try:
            log.debug("reporting success of %s", test)
            self.reporter.reportSuccess(test)
        except:
            info = sys.exc_info()
            try:
                self.reporter.reportSystemError(name = "Exception in zfsReportPlugin",
                    description = "Unexpected exception in ZfsReportPlugin.addSuccess",
                    errInfo = info)
            except:
                log.debug("exception when reporting success:\n%s", format_exc())
                    
    
    def finalize(self, result):
        """ Finalize reporting data (Calls finalize on ReportProxy)
            
            .. See: nose plugin interface
        """
        self.reporter.finalize()

