
from django.utils.translation import ugettext as _
from django.http import HttpResponse
from django.shortcuts import render_to_response, get_object_or_404
from models import BatchRun, Project
from django.core.paginator import ObjectPaginator, InvalidPage
from TestResultStorage.resultRepository.views import globalMenu

DEFAULT_PAGING = 20

def batchList(request,page = None, paging = None, projectId = None, branch = None):
    if not paging:
        paging = DEFAULT_PAGING
    else:
        paging = int(paging)
        if not paging: #ignore zero
            paging = DEFAULT_PAGING
            
    if page:
        page = int(page) - 1
    else:
        page = 0
    
    if projectId:
        project = Project.objects.get(id = projectId)
    else:
        project = None
        
    if project:
        latestBatches = BatchRun.objects.filter(project = project).order_by('-startTime')
    else:
        latestBatches = BatchRun.objects.all().order_by('-startTime')
        
    if branch:
        latestBatches = latestBatches.filter(branch = branch)
    
    selectedBatches = ObjectPaginator(latestBatches, paging)
    if page < 0 or page >= selectedBatches.pages:
        page = 0
    
    
    for batch in latestBatches:
        setattr(batch, "res", batch.get_result_display())
        setattr(batch, "fin", batch.hasFinished == 1) #translate int to boolean
        if batch.result > 0:
            setattr(batch, "resId", "failure")
        elif batch.result < 0:
            setattr(batch, "resId", "unknown")
        else:
            setattr(batch, "resId", "success")
    return render_to_response('resultRepository/batchrun_list.html',
                                                {
                                        'batchList': selectedBatches.get_page(page),
                                        'page' : page + 1,
                                        'pages' : selectedBatches.page_range,
                                        'batchCount' : selectedBatches.hits,
                                        'project' : project,
                                        'branch' : branch,
                                        'paging' : paging,
                                        'projectList' : Project.objects.all(),
                                        'globalMenu' : globalMenu,
                                                })
    
def batch(request, batchUuid):
    batch = get_object_or_404(BatchRun, batchUuid = batchUuid)
    return render_to_response('resultRepository/batchrun_detail.html', {'batch' : batch})
