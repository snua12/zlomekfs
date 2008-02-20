import logging
import os
import datetime
import socket
import traceback
import pickle
try:
    from _mysql_exceptions import Warning as  SqlWarning
except ImportError:
    class SqlWarning(Exception):
        pass

#TODO: report exceptions

from TestResultStorage.resultRepository.models import BatchRun, TestRun, TestRunData, Project, computeDuration

log = logging.getLogger ("nose.plugins.zfsReportPlugin")

class ReportProxy(object):
    try:
        from TestResultStorage.settings import MEDIA_ROOT
        dataDir = MEDIA_ROOT
    except ImportError:
        dataDir = '/tmp'
    
    
    startTimeAttr = "startTime"
    endTimeAttr = "endTime"

    def __init__(self):
    
        try:
            self.batch = BatchRun.objects.get(id = int(os.environ['BATCHUUID']))
            if not self.batch:
                raise KeyError('empty result')
            
        except KeyError: #no batch predefined
            self.batch = BatchRun()
            self.batch.startTime = datetime.datetime.now()
            self.batch.result = -2
            self.batch.project = Project.objects.get_or_create(projectName = 'Unknown')[0]
    #        self.batch.duration = 0
            self.batch.description = "report.py direct generated batch"
            self.batch.machineName = socket.gethostname()
    #        self.batch.repositoryRevision = 0
            
            self.batch.save()
        
        if not self.batch.id:
            log.error ("Error: batch id is null")
    
    def generateDefaultRun(self, test, duration = None, name = None, description = None):
        run = TestRun()
        run.batchId = self.batch
        if name:
            run.testName = name
        else:
            run.testName = str(test.test)
            
        if description:
            run.description = description
        elif hasattr(test.test, "shortDescription"):
            run.description = test.shortDescription()
            
        if hasattr(test.test, self.startTimeAttr):
            run.startTime = getattr(test.test, self.startTimeAttr)
        else:
            run.startTime = datetime.datetime.now()
            
        if duration:
            run.duration = duration
        elif hasattr(test.test, self.startTimeAttr):
            if hasattr(test.test, self.endTimeAttr):                
                run.duration = computeDuration(getattr(test.test, self.startTimeAttr),
                                getattr(test.test, self.endTimeAttr))
            else:
                run.duration = computeDuration(getattr(test.test, self.startTimeAttr),
                                datetime.datetime.now())
                                
            
        return run
    
    def reportSuccess(self, test, duration = None, name = None, description = None):
        run = self.generateDefaultRun(test, duration, name, description)
        run.result = 0
        
        try:
            run.save()
        except SqlWarning: # heuristic: truncate data
            run.description = run.description[:256]
            run.save()
            log.debug(traceback.format_exc())
            pass

    
    def reportFailure(self, failure, duration = None, name = None, description = None):
        run = self.generateDefaultRun(failure.test, duration, name, description)
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

