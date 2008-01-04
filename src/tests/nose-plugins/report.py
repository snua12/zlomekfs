from failure import ZfsTestFailure
import os

class ReportProxy(object):
    targetDirEnvOpt = "RESULT_DIR"
    targetDir = "/tmp"
    
    successFile = "successess"
    failFile = "failures"
    def __init__(self):
        try:
            self.targetDir =  os.environ[ReportProxy.targetDirEnvOpt]
        except KeyError:
            pass
        
    def reportSuccess(self, test):
        file = open(self.targetDir + os.sep + self.successFile, "w")
        file.write("test %s successed\n" % str(test))
        file.close()
    def reportFailure(self, failure):
        file = open(self.targetDir + os.sep + self.failFile, "w")
        file.write("test %s failed\n" % str(failure.test))
        file.write(str(failure.failure))
        file.close()
        if hasattr(failure.test, "test") and hasattr(failure.test.test, "snapshotBuffer"):
            for snapshot in failure.test.test.snapshotBuffer:
                snapshot.pack(self.targetDir + os.sep + "failureSnapshot-" + str(failure) + "-" + str(id(snapshot)))
        
        
