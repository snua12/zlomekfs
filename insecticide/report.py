import logging
import os
import datetime
import socket
import traceback

from TestResultStorage.resultRepository.models import BatchRun, TestRun, TestRunData, Project, computeDuration

log = logging.getLogger ("nose.plugins.zfsReportPlugin")

class ReportProxy(object):
    targetDir = '/tmp'
    
    
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
        
        run.save()
    
    def reportFailure(self, failure, duration = None, name = None, description = None):
        run = self.generateDefaultRun(failure.test, duration, name, description)
        run.result = 1
        
        run.save()
        
        runData = TestRunData()
        runData.runId = run
        runData.backtrace = str(traceback.format_tb(failure.failure[2]))
        runData.errText = str(traceback.format_exception_only(
                failure.failure[0], failure.failure[1]))
        
        if hasattr(failure.test, "test") and hasattr(failure.test.test, "snapshotBuffer"):
            for snapshot in failure.test.test.snapshotBuffer:
                snapshot.pack(self.targetDir + os.sep + "failureSnapshot-" + str(failure) + "-" + str(id(snapshot)))
                runData.dumpFile = self.targetDir + os.sep + "failureSnapshot-" + str(failure) + "-" + str(id(snapshot))
        
        
        runData.save()
