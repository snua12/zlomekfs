#!/bin/env python
""" Cleanup script for zfs testing data """


import os
import re
import time
import datetime
import shutil

from TestResultStorage import settings
from TestResultStorage.resultRepository.models import BatchRun, TestRun
from TestResultStorage.resultRepository.models import TestRunData


dateBeforeLunarMonth = datetime.datetime.now() - datetime.timedelta(days=1)
unixTimeBeforeLunarMonth = time.time() - 60 * 60 * 24 * 28
unixTimeBeforeDay = time.time() - 60 * 60 * 24

def removeOldBatches():
    """ Remove batches older than 28 days. """
    BatchRun.objects.exclude(startTime__gt = dateBeforeLunarMonth).delete()
    
    
def removeOldRunData():
    """ Remove RunData older than 28 days.
        (especially failure snapshots from disk)
    """
    whereStatement = 'runId_id in ('
    for oldRun in TestRun.objects.exclude(startTime__gt = dateBeforeLunarMonth):
        whereStatement += str(oldRun.id) + ','
        
    if len(whereStatement) == 10:
        return
    else:
        whereStatement = whereStatement[:len(whereStatement) - 1] + ')'
    for oldData in TestRunData.objects.extra(where=[whereStatement]):
        if oldData.get_dumpFile_filename():
            try:
                print "removing " + str(oldData.get_dumpFile_filename())
                os.unlink(oldData.get_dumpFile_filename())
            except OSError:
                pass
        oldData.delete()


def removeMatchedTrees(matchFunc, directory, files):
    """ Remove directory threes that matches given criteria.
        
        :Parameters:
            matchFunc: function taking one argument: full file name
            directory: current directory
            files: list of files (directories, links, etc) in given directory
    """
    for fileName in files:
        fullFileName = os.path.join(directory, fileName)
        if matchFunc(fullFileName):
            try:
                print "removing " + fullFileName
                if os.path.isdir(fullFileName):
                    shutil.rmtree(fullFileName)
                else:
                    os.unlink(fullFileName)
            except OSError:
                pass
            

def matchOldRpms(fileName):
    """ Match rpm files older than 28 days.
        
        :Parameters:
            fileName: full file name
            
        :Return:
            True if given file is rpm older than 28 days
            False otherwise
    """
    if fileName.endswith('.rpm'):
         if os.path.getmtime(fileName) < unixTimeBeforeLunarMonth:
             return True
    return False

tempDataMatch = re.compile(
    r'.*((testCompareDir.*)|(insecticide.*log.*)|(zfsTestTemp.*)|(zfsMountPoint.*))')
""" Regexp for matching insecticide and zfs test data. """

def matchOldTempData(fileName):
    """ Match insecticide / zfs test temp data older than day.
        
        :Parameters:
            fileName: full file name to match
            
        :Return:
            True if file matches tempDataMatch and is older than day.
            False otherwise
    """
    if tempDataMatch.match(fileName) \
        and os.path.getmtime(fileName) < unixTimeBeforeDay:
            return True
    return False

def cleanup():
    """ Remove old testing data including:
            * Entries in database older than 28 days
            * failure snapshots older than 28 days
            * rpms older than month
            * old testing data accidentally left in /tmp
    """
    print "cleaning RunData"
    removeOldRunData()
    
    print "cleaning Batches"
    removeOldBatches()
    
    print "cleaning temp data"
    os.path.walk('/tmp', removeMatchedTrees, matchOldTempData)
    
    print "cleaning rpms"
    os.path.walk(settings.MEDIA_ROOT, removeMatchedTrees, matchOldRpms)
    

if __name__ == '__main__':
    cleanup()