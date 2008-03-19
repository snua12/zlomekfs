""" Module with insecticide failure wrapper classes. """

class ZfsTestFailure(object):
    """ Wrapper for test failure (currently rather empty).
    """
    def __init__(self,  test,  failure):
        """ Set failure info.
            
            :Parameters:
                test: test which has failed
                    (the object which has inst, snapshotBuffer, etc)
                failure: sys.exc_info tuple
        """
        self.test = test
        self.failure = failure
        
    def delete(self):
        """ Remove all related data (such as snapshots) """
        if hasattr(self.test, 'snapshotBuffer'):
            while self.test.snapshotBuffer:
                self.test.snapshotBuffer.pop().delete()
        self.test = None
        self.failure = None
        
