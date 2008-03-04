""" Module with TestResultStorage wrapper for reporting nose test results into django db """

import logging
import os
import sys
import pysvn
import datetime
import traceback
import pickle

from TestResultStorage.resultRepository.models import BatchRun, TestRun, TestRunData
from TestResultStorage.resultRepository.models import  ProfileInfo, Project, computeDuration
from TestResultStorage.resultRepository.models import  RESULT_UNKNOWN
from TestResultStorage.resultRepository.models import  RESULT_FAILURE, RESULT_SUCCESS
from TestResultStorage.resultRepository.models import  RESULT_ERROR

from _mysql_exceptions import Warning as  SqlWarning

log = logging.getLogger ("nose.plugins.zfsReportPlugin")

#TODO: report exceptions

profileEnvOpt = 'PROFILE_NAME'
""" environment variable name from which to try to load name of module to load as profile """

slavenameEnvOpt = 'SLAVE_NAME'
""" Environment variable name from which to try to load name of this machine (buildslave) """

batchuuidEnvOpt = 'BATCHUUID'
""" Environment variable name from which to try to load batch id if batch is provided from outside """

projectnameEnvOpt = 'PROJECT_NAME'
""" Environment variable name from which to try to load project name. """

branchEnvOpt = 'BRANCH'
""" Environment variable name from which to try to load branch name (path). """

startTimeAttr = "startTime"
""" Name of attribute which will be used on test to store to (and load from) it's start time. """

endTimeAttr = "endTime"
""" Name of attribute which will be used on test to store to (and load from) it's end time. """

def loadProfile(batch, profileName):
    """ Try to import profile to current os.environ and set batch .profileInfo.
        Batch is saved upon return.
        
        :Parameters:
            batch: BatchRun instance where to store variables found to
            profileName: module name to import as profile
            
        :Return: 
            None
            
        :Raise:
            EnvironmentError: if module is not found
        """
    try:
        mod = __import__(profileName, {}, {}, [''])
    except ImportError, e:
        raise EnvironmentError, \
                    "Could not import profile '%s' (Is it on sys.path? Does it have syntax errors?): %s" \
                    % (profileName, e)
        
    profile = mod.env
    for key in profile.keys(): #note we override ALL commandline given args
        info = ProfileInfo.objects.get_or_create(variableName = str(key),
                                    variableValue = str(profile[key]))
        batch.profileInfo.add(info[0])
        
    os.environ.update(profile)
        
    batch.save()


def generateLocalBatch(project = None):
    """ Generates batch according settings found in os.environ.
        
        Looks for branchEnvOpt, slavenameEnvOpt, projectnameEnvOpt,
        profileEnvOpt. If profileEnvOpt is found, tries to load profile. 
        
        If subversion related variables are not found, tries to load them from cwd.
        
        :Parameters:
            project: project name. If None and not found in environment, will be set to 'Unknown'
        
        :Return:
            saved batch
    """
    try:
        svn = pysvn.Client()
        rootInfo = svn.info('.')
        
        repository = str(rootInfo.repos)
        if branchEnvOpt not in os.environ:
            # this could be broken (contain test dir too)
            branch = str(rootInfo.url)[len(repository) + 1:]
        else:
            branch = os.environ[branchEnvOpt]
        revision = rootInfo.revision.number
    except pysvn._pysvn_2_5.ClientError:
        repository = 'Exported'
        branch = 'Exported'
        revision = 0
        
    try:
        machineName = os.environ[slavenameEnvOpt]
    except KeyError:
        machineName = os.environ['HOSTNAME']
        os.environ[slavenameEnvOpt] = machineName
        
    try:
        profile = os.environ[profileEnvOpt]
    except KeyError:
        profile = 'profile_default'
        os.environ[profileEnvOpt] = profile
        
    if not project:
        try:
            project = os.environ[projectnameEnvOpt]
        except KeyError:
            project = 'Unknown'
            
    batch = BatchRun()
    
    batch.startTime = datetime.datetime.now()
    batch.result = RESULT_UNKNOWN
    project = Project.objects.get_or_create(projectName = project, sourceRepositoryUrl = repository)
    if project:
        batch.project = project[0]
    batch.branch = branch
    batch.repositoryRevision = revision
#    batch.duration = 0
    batch.description = "Batch for %s in rev %s" % (branch, revision)
    batch.machineName = machineName
    batch.profileName = profile
    
    batch.save()
    
    os.environ[batchuuidEnvOpt] = str(batch.id)
    if not batch.id:
        log.error("Error: batch id is null")
    else:
        try:
            loadProfile(batch, profile)
        except EnvironmentError:
            log.warning("Batch created without profile")
            log.warning(traceback.format_exc())
        
    return batch
    
def finalizeBatch(batchId = None):
    """ Finalizes given batch - counts duration, test count and result.
        
        :Parameters:
            batchId: id of batch to finalize. If None, tries to load it from environment.
            
        :Return:
            None
    """
    if not batchId:
        try:
            batchId = os.environ[batchuuidEnvOpt]
        except KeyError:
            log.error ("BatchId is not defined when finalizeBatch called.")
            return
    
    batch = BatchRun.objects.get(id = batchId)
    if batch:
        endTime = datetime.datetime.now()
        startTime = batch.startTime
    batch.duration = computeDuration(startTime, endTime)
    tests = TestRun.objects.filter (batchId = batch)
    if tests:
        batch.testCount = tests.count()
        
        if tests.filter(result = RESULT_SUCCESS).count():
            batch.result = RESULT_SUCCESS
        if tests.filter(result = RESULT_ERROR).count(): #errors
            batch.result = RESULT_ERROR
        elif tests.filter(result = RESULT_FAILURE).count(): #failures
            batch.result = RESULT_FAILURE
        else: #unknown tests.filter(result=-2).count()
            pass
    else: #we assume skipped tests as o.k.
        batch.result = RESULT_SUCCESS
        batch.testCount = 0
    
    batch.hasFinished = True
    
    batch.save()
    

def generateDefaultRun(batch, test = None, duration = None, name = None, description = None):
    """ Generate TestRun instance according to given arguments. 
        
        :Parameters:
            batch: batch to which test belongs
            test: nose Test object which contains TestCase instance if run belongs to real test run
                None otherwise (when reporting system errors, etc)
            duration: duration of test. If None, will be computed from test attributes.
            name: name of TestRun. If None, test.test.__str__ will be used
            description: description of TestRun. If None, test.shortDescription will be used
            
        :Return:
            saved TestRun instance or None
            
        :Raise:
            Exception: if batch is not defined
    """
    if not batch:
        raise Exception ("Batch id not defined (%s)" % batch)
    if not test:
        class fake:
            test = "fake"
        test = fake()
    run = TestRun()
    run.batchId = batch
    if name:
        run.testName = name
    elif test:
        run.testName = str(test.test)
    else:
        run.testName = "Unknown"
        
    if description:
        run.description = description
    elif test and hasattr(test.test, "shortDescription"):
        run.description = test.shortDescription()
    else:
        run.description = "Unknown"
        
    if hasattr(test.test, startTimeAttr):
        run.startTime = getattr(test.test, startTimeAttr)
    else:
        run.startTime = datetime.datetime.now()
        
    if duration:
        run.duration = duration
    elif hasattr(test.test, startTimeAttr):
        if hasattr(test.test, endTimeAttr):                
            run.duration = computeDuration(getattr(test.test, startTimeAttr),
                            getattr(test.test, endTimeAttr))
        else:
            run.duration = computeDuration(getattr(test.test, startTimeAttr),
                            datetime.datetime.now())
                            
        
    return run

def appendDataToRun(run, errInfo = None, dataDir = None, test = None):
    """ Appends auxiliary data to TestRun.
        
        :Parameters:
            run: TestRun instance
            errInfo: sys.exc_info() tuple describing exception
            dataDir: directory where to store big data (such as snapshots)
            test: if given, tries to find snapshots in it's snapshotBuffer
            
        :Return:
            None
    """
    runData = TestRunData()
    runData.runId = run
    if errInfo:
        runData.backtrace = pickle.dumps(traceback.format_tb(errInfo[2]), protocol = 0)
        runData.errText = traceback.format_exception_only(errInfo[0], errInfo[1])
        if len(runData.errText) == 1:
            runData.errText = runData.errText[0]
        else:
            runData.errText = str(runData.errText)
        #runData.errText = pickle.dumps(traceback.format_exception_only(errInfo[0], errInfo[1]), protocol = 0)
    
    if test and hasattr(test, "test") and hasattr(test.test, "snapshotBuffer"):
        snapshot = test.test.snapshotBuffer.pop()
        targetFileName = os.path.join(dataDir, "failureSnapshot-" + str(run.id) + "-" + str(id(snapshot)))
        snapshot.pack(targetFileName)
        snapshot.delete()
        runData.dumpFile = "failureSnapshot-" + str(run.id) + "-" + str(id(snapshot))
        log.debug("appending snapshot from dir '%s' into file '%s'", snapshot.directory, targetFileName)
        for snapshot in test.test.snapshotBuffer:
            snapshot.delete()
    else:
        runData.dumpFile = None
            
    runData.save()
    
    
def reportSystemError(batch, name = None, description = None, errInfo = None):
    """ Report system error (non-test)
        
        :Parameters:
            batch: batch to associate error with
            name: short name of error (such as exception name)
            description: description of error
            errInfo: sys.exc_info tuple
            
        :Return:
            None
        
        :Raise:
            Exception: if batch.id is not valid
    """
    if not batch:
        raise Exception("batch id not defined (%s)" % batch)
    if not name:
        name = "Unknown"
    if not description:
        description = "Unknown system error"
    
    run = generateDefaultRun(batch = batch, name = name, description = description)
    run.result = RESULT_ERROR
    run.duration = 0
    
    run.save()
    if not errInfo:
        errInfo = sys.exc_info()
        
    appendDataToRun(run = run , errInfo = errInfo)
    
    
    
class ReportProxy(object):
    """ Object holding batch information, serve as proxy for reporting test results.
    """
    batch = None
    """ BatchRun instance of batch to which tests should be reported """
    
    selfContainedBatch = False
    """ If batch was generated by us or inherited from upper level (True if it is own)
    """
    try:
        from TestResultStorage.settings import MEDIA_ROOT
        dataDir = MEDIA_ROOT
    except ImportError:
        dataDir = '/tmp'
    """ Directory where snapshot packages (and other big data) should be put to.
        TestResultStorage should be able to read them. """

    def __init__(self):
        """ Loads batch according os.environ or creates own
        """
        try:
            self.batch = BatchRun.objects.get(id = int(os.environ[batchuuidEnvOpt]))
            if not self.batch:
                raise KeyError('empty result')
        except KeyError: #no batch predefined
            self.batch = generateLocalBatch()
            self.selfContainedBatch = True
        
        if not self.batch.id:
            log.error ("Error: batch id is null")
        
    def finalize(self):
        """ Finalizes data. If we have our own batch (not creted from outside), finalizes it
        """
        if self.batch and self.selfContainedBatch:
            log.debug("finalizing self contained")
            finalizeBatch(self.batch.id)
        
    def reportSuccess(self, test, duration = None, name = None, description = None):
        """ Report success of test.
            
            :Parameters:
                test: Test object with TestCase instance
                
            :Return:
                None
                
            .. See generateDefaultRun
        """
        run = generateDefaultRun(batch = self.batch, test = test, duration = duration,
                                            name = name, description = description)
        run.result = RESULT_SUCCESS
        
        try:
            run.save()
        except SqlWarning: # heuristic: truncate data
            #note: this may generate extra report entry
            run.description = run.description[:256]
            run.save()
            log.debug(traceback.format_exc())
    
    
    def reportFailure(self, failure, duration = None, name = None, description = None, error = False):
        """ Report test failure (or error).
            
            :Parameters:
                failure: ZfsFailure instance holding test and sys.exc_info
                error: if reported item is failure (False) or error (True)
            
            :Return:
                None
                
            .. See generateDefaultRun
        """
        run = generateDefaultRun(batch = self.batch, test = failure.test,
                                    duration = duration, name = name, description = description)
        if error:
            run.result = RESULT_ERROR
        else:
            run.result = RESULT_FAILURE
        
        try:
            run.save()
        except SqlWarning: # heuristic: truncate data
            #note: this may generate extra report entry
            run.description = run.description[:256]
            run.save()
            log.debug(traceback.format_exc())
        
        appendDataToRun(run = run, errInfo = failure.failure, dataDir = self.dataDir, 
            test = failure.test)
    
    def reportError(self, failure, duration = None, name = None, description = None):
        """ Report error of test. Redirected to self.reportFailure with error = True
            
            .. See ReportProxy.reportFailure
        """
        return self.reportFailure(failure, duration, name, description, error = True)
    
    def reportSystemError(self, name = None, description = None, errInfo = None):
        """ Report system error in this batch run.
            
            .. See reportSystemError
        """
        return reportSystemError(self.batch, name, description, errInfo)
        
