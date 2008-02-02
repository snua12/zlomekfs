
from django.utils.translation import ugettext as _
from django.http import HttpResponse
from django.shortcuts import render_to_response, get_object_or_404
from models import BatchRun
    
def batchList(request):
    latestBatchs = BatchRun.objects.all().order_by('-startTime')[:20]
    batchCount = BatchRun.objects.count()
    return render_to_response('resultRepository/batchList.html',
                                                {
                                        'batchList': latestBatchs,
                                        'batchCount' : batchCount,
                                                })
    
def batch(request, batchUuid):
    batch = get_object_or_404(BatchRun, batchUuid = batchUuid)
    return render_to_response('resultRepository/batch.html', {'batch' : batch})
