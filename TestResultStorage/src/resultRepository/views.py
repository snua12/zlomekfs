
from django.utils.translation import ugettext as _

# internationalize string -- _(string)

globalMenu = "".join([
    '<div id = "leftList">',
        '<span id = "small-header">Goto:</span><br>',
        '<a href="/batches/">Batches</a><br/>',
        '<a href="/projects/">Projects</a><br/>',
        '<a href="/tests/">Tests</a><br/>',
    '</div>'])

def index(request):
    from batchViews import batchList
    return batchList(request)
