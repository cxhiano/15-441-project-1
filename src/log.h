/** @file log.h
 *  @brief Defines functions for logging
 *
 *  @author Chao Xin(cxin)
 */
#ifndef __MYLOG_H__
#define __MYLOG_H__

#include <stdio.h>

#define L_ERROR 0x1 //This flag indicates an error message
#define L_INFO 0x2 //This flag indicates an info message
/* User define debug flag */
#define L_IO_DEBUG 0x4
#define L_HTTP_DEBUG 0x8

/*
 * @brief log_file indicates where to write log. When it's NULL, log will be written
 *        to stderr
 */
FILE* log_file;
/*
 * @brief A bit map of what types of logs should be output
 */
int log_mask;

void set_log_file(char* fname);

void log_msg(int type, char* format, ...);
void log_error(char* msg);

#endif