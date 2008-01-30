
import os
import datetime
import uuid
import socket
import traceback

from TestResultStorage.resultRepository.models import BatchRun, TestRun, TestRunData

class ReportProxy(object):
    targetDir = '/tmp'

    def __init__(self):
    
        try:
            batchQuery = BatchRun.objects.filter(batchUuid = os.environ['BATCHUUID'])
            if batchQuery:
                self.batch = batchQuery[0]
            
        except KeyError: #no batch predefined
            self.batch = BatchRun()
            self.batch.startTime = datetime.datetime.now()
            self.batch.batchUuid = uuid.uuid1()
    #        self.batch.duration = 0
            self.batch.description = "report.py direct generated batch"
            self.batch.machineName = socket.gethostname()
    #        self.batch.repositoryRevision = 0
            
            self.batch.save()
        if not self.batch.id:
            print ("Error: batch id is null")
    
    def generateDefaultRun(self, test):
        run = TestRun()
        run.batchId = self.batch
        run.startTime = datetime.datetime.now()
        run.runUuid = uuid.uuid1()
        run.testName = str(test.test)
        run.duration = 15
        
        return run
    
    def reportSuccess(self, test):
        run = self.generateDefaultRun(test)
        run.result = 0
        
        run.save()
    
    def reportFailure(self, failure):
        run = self.generateDefaultRun(failure.test)
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
