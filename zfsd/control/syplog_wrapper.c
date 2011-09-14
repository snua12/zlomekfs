#include "syplog_wrapper.h"
#include "syplog.h"

syp_error control_wrap_set_log_level (logger glogger, log_level_t level)
{
  return set_log_level(glogger, level);
}

syp_error control_wrap_set_facility (logger glogger, facility_t facility)
{
  return set_facility(glogger, facility);
}

syp_error control_wrap_reset_facility (logger glogger, facility_t facility)
{
  return reset_facility(glogger, facility);
}

