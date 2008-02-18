from django.conf.urls.defaults import *
from django.conf import settings
from resultRepository.views import testDetailDir, testListDir, batchDetailDir, batchListDir, projectListDir, dataDir

urlpatterns = patterns('',
    # index page
    (r'^$', 'TestResultStorage.resultRepository.views.index'),
    # list of batches
    (r'^' + batchListDir + '/$', 'TestResultStorage.resultRepository.batchViews.batchList'),
    # batch details
    (r'^' + batchDetailDir + '/$', 'TestResultStorage.resultRepository.batchViews.batchDetail'),
    # list of tests
    (r'^' + testListDir + '/$', 'TestResultStorage.resultRepository.testViews.testList'),
    # details of test
    (r'^' + testDetailDir + '/$', 'TestResultStorage.resultRepository.testViews.testDetail'),
    # list of projects
    (r'^' + projectListDir + '/$', 'TestResultStorage.resultRepository.projectViews.projectList'),
    # admin page
    (r'^admin/', include('django.contrib.admin.urls')),
    # i18n
    (r'^i18n/', include('django.conf.urls.i18n')),
    # templates
    #NOTE: this path is hardcoded in templates
    (r'^webMedia/(?P<path>.*)$', 'django.views.static.serve', {'document_root': settings.WEB_MEDIA_ROOT}),
    # uploaded files
    (r'^' + dataDir + '/(?P<path>.*)$', 'django.views.static.serve', {'document_root': settings.MEDIA_ROOT}),

)


if settings.DEBUG:
    urlpatterns += patterns('',
        (r'^admin/resultRepository/testrundata/[0-9]+/(?P<path>.*)$', 'django.views.static.serve', {'document_root': settings.MEDIA_ROOT}),
    )
