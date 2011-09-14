#ifndef SYPLOG_WRAPPER_H
#define SYPLOG_WRAPPER_H

#include "syplog.h"

// wrapper for set_log_level
syp_error control_wrap_set_log_level(logger glogger, log_level_t level);

// wrapper for set_facility
syp_error control_wrap_set_facility(logger glogger, facility_t facility);

// wrapper for reset_facility
syp_error control_wrap_reset_facility(logger glogger, facility_t facility);

#endif
