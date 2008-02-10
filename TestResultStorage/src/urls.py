from django.conf.urls.defaults import *
from django.conf import settings
from resultRepository.views import testDetailDir, testListDir, batchDetailDir, batchListDir, projectListDir

urlpatterns = patterns('',
    # Example:
    # (r'^TestResultStorage/', include('TestResultStorage.foo.urls')),

    # Uncomment this for admin:
    (r'^$', 'TestResultStorage.resultRepository.views.index'),
    (r'^' + batchListDir + '/$', 'TestResultStorage.resultRepository.batchViews.batchList'),
    (r'^' + batchDetailDir + '/$', 'TestResultStorage.resultRepository.batchViews.batchDetail'),
    (r'^' + testListDir + '/$', 'TestResultStorage.resultRepository.testViews.testList'),
    (r'^' + testDetailDir + '/$', 'TestResultStorage.resultRepository.testViews.testDetail'),
    (r'^' + projectListDir + '/$', 'TestResultStorage.resultRepository.projectViews.projectList'),
    (r'^admin/', include('django.contrib.admin.urls')),
    (r'^i18n/', include('django.conf.urls.i18n')),
    (r'^webMedia/(?P<path>.*)$', 'django.views.static.serve', {'document_root': settings.WEB_MEDIA_ROOT}),
    (r'^data/(?P<path>.*)$', 'django.views.static.serve', {'document_root': settings.MEDIA_ROOT}),

)


if settings.DEBUG:
    urlpatterns += patterns('',
        (r'^admin/resultRepository/testrundata/[0-9]+/(?P<path>.*)$', 'django.views.static.serve', {'document_root': settings.MEDIA_ROOT}),
    )
