""" Module with django database models description """

from django.db import models
from django.utils.translation import ugettext_lazy as _

from TestResultStorage import settings

# length of uuid field
UUID_LEN = 36
# default length of name (any)
NAME_LEN = 64
# length of test name
TEST_NAME_LEN = NAME_LEN
# length of machine name (hostname)
MACHINE_NAME_LEN = NAME_LEN
# length of batch description
#TEST_DESC_LEN = 256
# length of file name
FILE_NAME_LEN = 100
# length of profile name
PROFILE_NAME_LEN = 100
# where to put dumps
DUMP_DIRECTORY = settings.MEDIA_ROOT
# length of environment variable value
ENV_LEN = 254 # for PATHs

def computeDuration(startTime, endTime):
    """ Compute duration of time segment in miliseconds
        Ignores month and above.
        
        :Parameters:
            startTime: datetime.datetime.now() output - start of time segment
            endTime: datetime.datetime.now() output - end of time segment
        :Return:
            integer - length in miliseconds
    """
    duration = endTime.day - startTime.day
    duration = duration * 24 + endTime.hour - startTime.hour
    duration = duration * 60 + endTime.minute - startTime.minute
    duration = duration * 60 + endTime.second - startTime.second
    duration = duration * 1000 + (endTime.microsecond - startTime.microsecond) / 1000
    
    return duration


class ProfileInfo(models.Model):
    """Object holding one-liner: environment variable name and value (Django database wrapper)
    
        .. See: verbose_name for field descriptions.
    """
    variableName = models.CharField(max_length = NAME_LEN, unique = False,
                verbose_name = _("Environment variable name"))
    variableValue = models.CharField(max_length = ENV_LEN, unique = False,
                verbose_name = _("Environment variable value"))
    pass
    class Meta:
        unique_together = ("variableName", "variableValue")
    class Admin:
        pass
        
    def __unicode__(self):
        return self.variableName + "=" + self.variableValue

class Project(models.Model):
    """Object holding project information: name and repository url. (Django database wrapper)
    
        .. See: verbose_name for field descriptions.
    """
    sourceRepositoryUrl = models.URLField(verify_exists = False,
                verbose_name = _("Url to repository from which sources has been fetched"),
                blank = True)
    projectName = models.CharField(max_length = NAME_LEN, verbose_name = _("Project name"),
                db_index = True)
    class Admin:
        pass
    class Meta:
        unique_together = (("projectName", "sourceRepositoryUrl"),)
        
    def __unicode__(self):
        return self.projectName
   
TEST_RESULT_CHOICES = (
    (0, 'Success'),
    (1, 'Failure'),
    (2, 'Error'),
    (-1, 'Skipped'),
    (-2, 'Unknown'),
) 
""" Possible test results. """

class BatchRun(models.Model):
    """Object holding information about batch run. (Django database wrapper)
    
    Batch run is collection of tests runned in one batch.
    This object describes common setup for all tests.
        .. See: verbose_name for field descriptions.
    """
    startTime = models.DateTimeField(
                verbose_name = _("Date and time when the BatchRUn has started"),
                db_index = True)
    duration = models.PositiveIntegerField(
                verbose_name = _("Duration of batch (should be set at the end or derived from last test values)"),
                blank = True, null = True)
    result = models.IntegerField(choices=TEST_RESULT_CHOICES,
                verbose_name = _("If test has successed, failed or what"),
                db_index = True)
    hasFinished = models.BooleanField(verbose_name = _("If this batch still runs or not"),
                default = False)
    testCount = models.IntegerField( verbose_name = _("How many tests run in this batch."),
                blank = True, null = True)
    project = models.ForeignKey(Project)
    branch = models.CharField(max_length = FILE_NAME_LEN,
                verbose_name = _("Branch in project repository"),
                blank = True, db_index = True)
    repositoryRevision = models.PositiveIntegerField(
                verbose_name = _("Revision of sources build for tests"),
                blank = True, null = True)
    description = models.TextField(verbose_name = _("Short description of batch"),
                blank = True) #max_length = TEST_DESC_LEN,
    profileName = models.CharField(max_length = PROFILE_NAME_LEN,
                verbose_name = _("Profile in which sources had been build for tests"),
                blank = True, db_index = True)
    profileInfo = models.ManyToManyField(ProfileInfo, blank = True)
    machineName = models.CharField(max_length = MACHINE_NAME_LEN,
                verbose_name = _("Hostname of machine on which has this batch run"),
                blank = True, db_index = True)
    
    def __unicode__(self):
        if self.repositoryRevision:
            return self.project.__unicode__() + ":" + str(self.repositoryRevision) + " (" + str(self.startTime) + ")"
        else:
            return self.project.__unicode__() + " (" + str(self.startTime) + ")"
        
    class Meta:
        get_latest_by = "order_startTime"
        ordering = ['-startTime', 'hasFinished']
    
    class Admin:
        pass

class TestRun(models.Model):
    """Object holding information about one test run. (Django database wrapper)
    
        .. See: verbose_name for field descriptions.
    """
    batchId = models.ForeignKey(BatchRun, verbose_name = _("Batch in which this test has run"),
                db_index = True)
    startTime = models.DateTimeField(verbose_name = _("Date and time when the test has started"))
    duration = models.PositiveIntegerField(verbose_name = _("Duration of test run in miliseconds"))
    testName = models.CharField(max_length = TEST_NAME_LEN,
                verbose_name = _("Name of test ie module.class.testName or simple testName"),
                db_index = True)
    description = models.TextField(verbose_name = _("Short description of test run"),
                blank = True, null = True) # max_length = TEST_DESC_LEN,
    result = models.IntegerField(choices=TEST_RESULT_CHOICES,
                verbose_name = _("If test has successed, failed or what"),
                db_index = True)
    sourceRepositoryPath = models.CharField(max_length = FILE_NAME_LEN,
                verbose_name = _("Path to file containing the test relative to BatchRun.sourceRepositoryUrl"),
                blank = True,
                db_index = True)
    
    def __unicode__(self):
        return self.testName
    
    class Meta:
        get_latest_by = "order_startTime"
        order_with_respect_to = 'batchId'
        ordering = ['batchId', '-startTime']
    
    class Admin:
        pass

class TestRunData(models.Model):
    """ Object holding information about data related to some test run. (Django database wrapper)
    
    Contains backtrace, error text (if any) and dump file path.
    
    .. See: verbose_name for field descriptions.
    """
    runId = models.ForeignKey(TestRun, verbose_name = _("Test run which generates this data"),
                db_index = True)
    dumpFile = models.FileField(verbose_name = _("Tar with dumped test run data (snapshots, etc)"),
                max_length = FILE_NAME_LEN, upload_to = DUMP_DIRECTORY,
                blank = True, unique = True)
    backtrace = models.TextField(verbose_name = _("Backtrace of failure (if present)"),
                blank = True)
    errText = models.TextField(verbose_name = _("Error text generated by test"),
                blank = True)
    
    def __unicode__(self):
        return "data " + str(self.id) + " for test " + str(self.runId)
        
    class Meta:
        get_latest_by = "order_runId"
        order_with_respect_to = 'runId'
        ordering = ['runId']
    
    class Admin:
        pass
        
