from django.db import models
from django.utils.translation import ugettext_lazy as _



UUID_LEN = 36
NAME_LEN = 64
TEST_NAME_LEN = NAME_LEN
MACHINE_NAME_LEN = NAME_LEN
TEST_DESC_LEN = 256
FILE_NAME_LEN = 100
PROFILE_NAME_LEN = 100
DUMP_DIRECTORY = "dumps"
ENV_LEN = 128

class ProfileInfo(models.Model):
    variableName = models.CharField(max_length = ENV_LEN, unique = False,
                verbose_name = _("Environment variable name"))
    variableValue = models.CharField(max_length = ENV_LEN, unique = False,
                verbose_name = _("Environment variable value"))
    pass
    class Meta:
        unique_together = ("variableName", "variableValue")

class BatchRun(models.Model):
    startTime = models.DateTimeField(
                verbose_name = _("Date and time when the BatchRUn has started"),
                db_index = True)
    duration = models.PositiveIntegerField(
                verbose_name = _("Duration of batch (should be set at the end or derived from last test values)"),
                blank = True, null = True)
    hasFinished = models.BooleanField(verbose_name = _("If this batch still runs or not"),
                default = False)
    batchUuid = models.CharField(max_length = UUID_LEN, unique = True,
                verbose_name = _("UUid for this batch - generated by runner in time of start"))
    sourceRepositoryUrl = models.URLField(verify_exists = False,
                verbose_name = _("Url to repository from which sources has been fetched"),
                blank = True, db_index = True)
    repositoryRevision = models.PositiveIntegerField(
                verbose_name = _("Revision of sources build for tests"),
                blank = True, null = True)
    description = models.CharField(max_length = TEST_DESC_LEN,
                verbose_name = _("Short description of batch"),
                blank = True)
    profileName = models.CharField(max_length = PROFILE_NAME_LEN,
                verbose_name = _("Profile in which sources had been build for tests"),
                blank = True, db_index = True)
    profileInfo = models.ManyToManyField(ProfileInfo, blank = True)
    machineName = models.CharField(max_length = MACHINE_NAME_LEN,
                verbose_name = _("Hostname of machine on which has this batch run"),
                blank = True, db_index = True)
    
    def __unicode__(self):
        return self.batchUuid
        
    class Meta:
        get_latest_by = "order_startTime"
        ordering = ['-startTime', 'hasFinished']
    
    class Admin:
        pass

TEST_RESULT_CHOICES = (
    (0, _('Success')),
    (1, _('Failure')),
    (2, _('Error')),
    (-1, _('Skipped')),
    (-2, _('Unknown')),
)

class TestRun(models.Model):
    batchId = models.ForeignKey(BatchRun, verbose_name = _("Batch in which this test has run"),
                db_index = True)
    startTime = models.DateTimeField(verbose_name = _("Date and time when the test has started"))
    duration = models.PositiveIntegerField(verbose_name = _("Duration of test run in miliseconds"))
    runUuid = models.CharField(max_length = UUID_LEN, unique = True,
                verbose_name = _("Uuid for this test (generated by runner in time of report"))
    testName = models.CharField(max_length = TEST_NAME_LEN,
                verbose_name = _("Name of test ie module.class.testName or simple testName"),
                db_index = True)
    description = models.CharField(max_length = TEST_DESC_LEN,
                verbose_name = _("Short description of test run"),
                blank = True)
    result = models.PositiveIntegerField(choices=TEST_RESULT_CHOICES,
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
        
