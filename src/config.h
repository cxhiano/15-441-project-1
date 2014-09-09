/** @fine config.h
 *  @brief Define global variable for configuration
 *  @author Chao Xin(cxin)
 */
#ifndef __CONFIG_H__
#define __CONFIG_H__

unsigned short http_port;
unsigned short https_port;
char* log_file_name,
      lock_file,
      www_folder,
      cgi_path,
      private_key_file,
      certificate_file;

#endif