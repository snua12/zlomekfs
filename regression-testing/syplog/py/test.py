
import pysyplog
log = pysyplog.logger_def()
pysyplog.open_log (log, "pylog",0,None)
pysyplog.do_log(log,pysyplog.LOG_ALERT,pysyplog.FACILITY_DATA,"ahoj")
pysyplog.close_log(log)
