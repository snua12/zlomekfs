##
# Fsx filesystem testing implementation
# Linux/unix dependent.
##

##
# operation read constant
OP_READ = 1
##
# operation write constant
OP_WRITE = 2
##
# operation truncate constant
OP_TRUNCATE = 3
##
# operation close and open constant
OP_CLOSEOPEN = 4
##
# operation map read constant
OP_MAPREAD = 5
##
# operation map write constant
OP_MAPWRITE = 6
##
# operation skip constant
OP_SKIPPED = 7

##
# Class holding log info.

class LogEntry:
    
    ##
    # int - operation associated to this log
    operation
    
    ##
    # int[3] some arguments ? TODO: specific meaning
    args
    
    ##
    # default constructor
    def __init__(self):
        self.operation = OP_NONE
        self.args = [0, 0, 0]
    def clone(log)
        args = log.args
        operation = log.operation


##
# This is fsx rewritement in python Class.
# I'm writing it manly for know, what can I do with python py.test.
# It's aimed on unix/ linux platform and use some platform dependant
# functions.

class TestFsx(object):

    ##
    # How much last operation to have in log.
    LOGSIZE = 1000

    ##
    # LogEntry[] - cyclic buffer of logs
    oplog = LogEntry[LOG_SIZE]
    ##
    # current possition in log
    logptr = 0
    ## 
    # total opts executed
    logcount = 0

    ##
    # int -  size of page
    page_size
    ##
    # int - mask to prevent writing outside
    page_mask

    ##
    # data string - pointer to the original data
    original_buf
    ##
    # data string - pointer to the correct data
    good_buf
    ##
    # data string - pointer to the current data
    temp_buf

    ##
    # string - name of our test file
    fname

    ##
    # handle of the opened test file
    fd

    ##
    # int - actual file size
    file_size = 0
    ##
    # int - maximal file size
    biggest = 0
    ##
    # data string - state? TODO: meaning?
    state
    
    ##
    # int - number of calls to function test
    testcalls = 0

unsigned long	simulatedopcount = 0;	/* -b flag */
int	closeprob = 0;			/* -c flag */
int	debug = 0;			/* -d flag */
unsigned long	debugstart = 0;		/* -D flag */
unsigned long	maxfilelen = 256 * 1024;	/* -l flag */
int	sizechecks = 1;			/* -n flag disables them */
int	maxoplen = 64 * 1024;		/* -o flag */
int	quiet = 0;			/* -q flag */
unsigned long progressinterval = 0;	/* -p flag */
int	readbdy = 1;			/* -r flag */
int	style = 0;			/* -s flag */
int	truncbdy = 1;			/* -t flag */
int	writebdy = 1;			/* -w flag */
long	monitorstart = -1;		/* -m flag */
long	monitorend = -1;		/* -m flag */
int	lite = 0;			/* -L flag */
long	numops = -1;			/* -N flag */
int	randomoplen = 1;		/* -O flag disables it */
int	seed = 1;			/* -S flag */
int     mapped_writes = 1;	      /* -W flag disables */
int 	mapped_reads = 1;		/* -R flag disables it */
int	fsxgoodfd = 0;
FILE *	fsxlogf = NULL;
int badoff = -1;
int closeopen = 0;
    def setup_method(self, method):

        self.alist = [5, 2, 3, 1, 4]



    def test_ascending_sort(self):

        self.alist.sort()

        assert self.alist == [1, 2, 3, 4, 5]



    def test_custom_sort(self):

        def int_compare(x, y):

            x = int(x)

            y = int(y)

            return x - y

        self.alist.sort(int_compare)

        assert self.alist == [1, 2, 3, 4, 5]



        b = ["1", "10", "2", "20", "100"]

        b.sort()

        assert b == ['1', '10', '100', '2', '20']

        b.sort(int_compare)

        assert b == ['1', '2', '10', '20', '100']



    def test_sort_reverse(self):

        self.alist.sort()

        self.alist.reverse()

        assert self.alist == [5, 4, 3, 2, 1]



    def test_sort_exception(self):

        import py.test

        py.test.raises(NameError, "self.alist.sort(int_compare)")

        py.test.raises(ValueError, self.alist.remove, 6)