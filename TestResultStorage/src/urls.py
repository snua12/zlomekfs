from django.conf.urls.defaults import *
from django.conf import settings
from TestResultStorage.resultRepository.models import TestRun

urlpatterns = patterns('',
    # Example:
    # (r'^TestResultStorage/', include('TestResultStorage.foo.urls')),

    # Uncomment this for admin:
    (r'^$', 'TestResultStorage.resultRepository.views.index'),
    (r'^batches/$', 'TestResultStorage.resultRepository.batchViews.batchList'),
    (r'^batches/(?P<page>\d*)/$', 'TestResultStorage.resultRepository.batchViews.batchList'),
    (r'^batches/(?P<page>\d*)/(?P<projectId>\d*)/$', 'TestResultStorage.resultRepository.batchViews.batchList'),
    (r'^batches/(?P<page>\d*)/(?P<projectId>\d*)/(?P<branch>[0-9a-zA-Z\/\\_-]*)/$', 'TestResultStorage.resultRepository.batchViews.batchList'),
    (r'^batch/(?P<batchUuid>[a-z0-9-]+)/$', 'TestResultStorage.resultRepository.batchViews.batch'),
    (r'^admin/', include('django.contrib.admin.urls')),
    (r'^i18n/', include('django.conf.urls.i18n')),
    (r'^webMedia/(?P<path>.*)$', 'django.views.static.serve', {'document_root': settings.WEB_MEDIA_ROOT}),

)


if settings.DEBUG:
    urlpatterns += patterns('',
        (r'^admin/resultRepository/testrundata/[0-9]+/(?P<path>.*)$', 'django.views.static.serve', {'document_root': settings.MEDIA_ROOT}),
    )
