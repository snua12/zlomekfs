/**
 *  \file coding.h
 * 
 *  \brief Coding style notes
 *  \author Ales Snuparek based on Jiri Zouhar thesis
 *
 */

/*! \page coding-style ZlomekFS coding conventions
 *
 *  \section c_style C based code
 *  For code in C, original formatting from ZlomekFS was adopted.
 *  Identifiers are in lower case, words separated by underscore.
 *  \code 
 *  uint32_t log_level;
 *  \endcode
 *  Defines (macros) are in upper case, words separated by underscore.
 *  \code
 *  #define MY_MACRO_CONSTANT 5
 *  \endcode
 *  Typedef's are in lower case with sux_t.
 *  \code
 *  typedef uint32_t fibheapkey_t;
 *  \endcode
 *  Braces around code block should be on new lines, indentation level as
 *  previous code.
 *  \code
 *  syp_error set_log_level (logger target, log_level_t level)
 *  {
 *  	target->log_level = level;
 *  	return NOERR;
 *  }
 *  \endcode
 *  Braces around function arguments should be separated from function
 *  name by one space, if argument list is multiline, ending brace should be right
 *  after last argument (on same line).
 *  \code
 *  syp_error send_uint32_by_function (uint32_t data,
 *  		syp_error (*function) (int, uint32_t, const struct sockaddr *, socklen_t),
 *  		const char * ip, uint16_t port);
 *  \endcode
 *  Indentation should be one tab per level.
 *  \code
 *  syp_error dbus_disconnect(DBusConnection ** connection)
 *  {
 *  	if (connection == NULL)
 *  		return ERR_BAD_PARAMS;
 *
 *  	if (*connection == NULL)
 *  		return ERR_NOT_INITIALIZED;
 *
 *  	dbus_bus_release_name (*connection,
 *
 *  	SYPLOG_DEFAULT_DBUS_SOURCE, NULL);
 *
 *  	dbus_connection_unref(*connection);
 *
 *  	*connection = NULL;
 *
 *  	return NOERR;
 *  }
 *  \endcode
 *  Operators should be separated from arguments by one space on both
 *  sides.
 *  \code
 *  file_position += bytes_written;
 *  \endcode
 *  Comments have one space between comment mark and comment text.
 *  They are on line before code they are describing.
 *  \code
 *  /// Structure holding logger state and configuration.
 *  typedef struct logger_def
 *  {
 *  /// input - output medium definition struct
 *  struct medium_def printer;
 *  \endcode
 *  File names consisting from more words should have dash between words.
 *  control-protocol.h
 *
 *  Indent style is based on Allman style http://en.wikipedia.org/wiki/Indent_style#Allman_style.
 *  This style can be enforced by "indent -bap -bli0 -i4 -l79 -ncs -npcs -npsl -fca -lc79 -fc1 -ts4"
 *
 *  \section python_style Python code
 *  TBD
*/

