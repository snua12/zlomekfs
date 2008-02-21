import logging
import os
import pysvn
import datetime
import socket
import traceback
import pickle
try:
    from _mysql_exceptions import Warning as  SqlWarning
except ImportError:
    class SqlWarning(Exception):
        pass

from TestResultStorage.resultRepository.models import BatchRun, TestRun, TestRunData, ProfileInfo, Project, computeDuration

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
    for key in profile.keys():
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
    batch.result = -2
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
        
        if tests.filter(result = 0).count():
            batch.result = 0
        if tests.filter(result=2).count(): #errors
            batch.result = 2
        elif tests.filter(result=1).count(): #failures
            batch.result = 1
        else: #unknown tests.filter(result=-2).count()
            pass
    else: #we assume skipped tests as o.k.
        batch.result = 0
        batch.testCount = 0
    
    batch.hasFinished = True
    
    batch.save()
    
def generateDefaultRun(batch, test, duration = None, name = None, description = None):
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
        run.result = 0
        
        try:
            run.save()
        except SqlWarning: # heuristic: truncate data
            run.description = run.description[:256]
            run.save()
            log.debug(traceback.format_exc())
            pass

    
    def reportFailure(self, failure, duration = None, name = None, description = None, error = False):
        run = generateDefaultRun(batch = self.batch, test = failure.test,
                                    duration = duration, name = name, description = description)
        if error:
            run.result = 2
        else:
            run.result = 1
        
        try:
            run.save()
        except SqlWarning: # heuristic: truncate data
            run.description = run.description[:256]
            run.save()
            log.debug(traceback.format_exc())
            pass
        
        runData = TestRunData()
        runData.runId = run
        runData.backtrace = pickle.dumps(traceback.format_tb(failure.failure[2]), protocol = 0)
        log.debug("backtrace saved: %s", runData.backtrace)
        runData.errText = traceback.format_exception_only(
                failure.failure[0], failure.failure[1])[0]
        
        if hasattr(failure.test, "test") and hasattr(failure.test.test, "snapshotBuffer"):
            snapshot = failure.test.test.snapshotBuffer.pop()
            snapshot.pack(self.dataDir + os.sep + "failureSnapshot-" + str(runData.id))
            runData.dumpFile = "failureSnapshot-" + str(failure) + "-" + str(id(snapshot))
        
        try:
            runData.save()
        except SqlWarning: #ignore truncation warnings
            log.debug(traceback.format_exc())
            pass
    
    def reportError(self, failure, duration = None, name = None, description = None):
        return self.reportFailure(failure, duration, name, description, error = True)
