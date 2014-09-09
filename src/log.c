/** @file log.c
 *  @brief Contains functions for logging
 *
 *  Each log message will come with a type. And we can configure what types of
 *  message should be logged by setting variable log_mask. Message type is
 *  defined in log.h. log_mask is basically a bit map of what message should be
 *  logged. By setting log_mask, we can output a specific part of logs.
 *
 *  @author Chao Xin(cxin)
 *
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "log.h"

FILE *log_file = NULL;
int log_mask = L_ERROR; //By default, only error message will be logged.

/** @brief Set the file where logs will be output to
 *
 *  @param fname The log file name
 *  @return void
 */
void set_log_file(char *fname) {
    FILE *f;

    if ((f = fopen(fname, "w")) == NULL) {
        log_error("set_log_file error");
        return;
    }
    log_file = f;
}

/** @brief Write formatted logs
 *
 *  Create a new log message with given type(Defined in log.h). The message
 *  will be written into log file if the type is in log_mask.
 *
 *  The output in this function is implemented using vfprintf. Thus this
 *  function can be used like printf(char* format, ... )
 *
 *  When log_file is NULL, the message will be output to stderr
 *
 *  @param type The type of the log message
 *  @param format The output format
 *  @oaram ... Arguments for the format
 *
 *  @return void
 */
void log_msg(int type, char* format, ...) {
    va_list arguments;

    if ((type & log_mask) != 0) {
        va_start(arguments, format);
        if (log_file == NULL)
            vfprintf(stderr, format, arguments);
        else
            vfprintf(log_file, format, arguments);
        va_end(arguments);
    }

    fflush(log_file);
}

/** @brief Write an error log
 *
 *  Works like perror(). The string explanation of errno will be appended to the
 *  passed-in message.
 *
 *  @param msg - The message about the error
 *
 *  @return void
 */
void log_error(char* msg) {
    log_msg(L_ERROR, "%s : %s\n", msg, strerror(errno));
}