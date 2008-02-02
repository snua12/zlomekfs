
from django.utils.translation import ugettext as _

# internationalize string -- _(string)

from batchViews import batchList

def index(request):
    return batchList(request)
