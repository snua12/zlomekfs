import logging
import os
import sys
import pysvn
import datetime
import traceback
import pickle

try:
    from _mysql_exceptions import Warning as  SqlWarning
except ImportError:
    class SqlWarning(Exception):
        pass

from TestResultStorage.resultRepository.models import BatchRun, TestRun, TestRunData
from TestResultStorage.resultRepository.models import  ProfileInfo, Project, computeDuration
from TestResultStorage.resultRepository.models import  RESULT_UNKNOWN
from TestResultStorage.resultRepository.models import  RESULT_FAILURE, RESULT_SUCCESS
from TestResultStorage.resultRepository.models import  RESULT_ERROR

log = logging.getLogger ("nose.plugins.zfsReportPlugin")

#TODO: report exceptions

profileEnvOpt = 'PROFILE_NAME'
slavenameEnvOpt = 'SLAVE_NAME'
batchuuidEnvOpt = 'BATCHUUID'
projectnameEnvOpt = 'PROJECT_NAME'
branchEnvOpt = 'BRANCH'

startTimeAttr = "startTime"
endTimeAttr = "endTime"

def loadProfile(batch, profileName):
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
    else:
        run.testName = str(test.test)
        
    if description:
        run.description = description
    elif hasattr(test.test, "shortDescription"):
        run.description = test.shortDescription()
        
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
        runData.dumpFile = targetFileName
        log.debug("appending snapshot from dir '%s' into file '%s'", snapshot.directory, targetFileName)
        for snapshot in test.test.snapshotBuffer:
            snapshot.delete()
    else:
        runData.dumpFile = None
            
    runData.save()
    
    
def reportSystemError(batch, name = None, description = None, errInfo = None):
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
    batch = None
    selfContainedBatch = False
    try:
        from TestResultStorage.settings import MEDIA_ROOT
        dataDir = MEDIA_ROOT
    except ImportError:
        dataDir = '/tmp'

    def __init__(self):
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
        # if we have our own batch (not creted from outside, we must finalize it too)
        if self.batch and self.selfContainedBatch:
            log.debug("finalizing self contained")
            finalizeBatch(self.batch.id)
        
    def reportSuccess(self, test, duration = None, name = None, description = None):
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
        return self.reportFailure(failure, duration, name, description, error = True)
