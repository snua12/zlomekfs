from django.conf.urls.defaults import *
from django.conf import settings

urlpatterns = patterns('',
    # Example:
    # (r'^TestResultStorage/', include('TestResultStorage.foo.urls')),

    # Uncomment this for admin:
    (r'^admin/', include('django.contrib.admin.urls')),
    (r'^i18n/', include('django.conf.urls.i18n')),
)


if settings.DEBUG:
    urlpatterns += patterns('',
        (r'^admin/resultRepository/testrundata/[0-9]+/(?P<path>.*)$', 'django.views.static.serve', {'document_root': settings.MEDIA_ROOT}),
    )
