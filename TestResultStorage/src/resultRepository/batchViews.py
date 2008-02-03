
from django.utils.translation import ugettext as _
from django.http import HttpResponse
from django.shortcuts import render_to_response, get_object_or_404
from models import BatchRun, Project
    
def batchList(request, projectId = None):
    if projectId:
        project = Project.objects.get(id = projectId)
    else:
        project = None
        
    if project:
        latestBatchs = BatchRun.objects.filter(project = project).order_by('-startTime')
    else:
        latestBatchs = BatchRun.objects.all().order_by('-startTime')
        
    batchCount = len(latestBatchs)
    for batch in latestBatchs:
        setattr(batch, "res", batch.get_result_display())
        setattr(batch, "fin", batch.hasFinished == 1) #translate int to boolean
        if batch.result > 0:
            setattr(batch, "resId", "failure")
        elif batch.result < 0:
            setattr(batch, "resId", "unknown")
        else:
            setattr(batch, "resId", "success")
    return render_to_response('resultRepository/batchList.html',
                                                {
                                        'batchList': latestBatchs[:20],
                                        'batchCount' : batchCount,
                                        'project' : project,
                                        'projectList' : Project.objects.all(),
                                                })
    
def batch(request, batchUuid):
    batch = get_object_or_404(BatchRun, batchUuid = batchUuid)
    return render_to_response('resultRepository/batch.html', {'batch' : batch})
