/*
Nintendo Switch FTP Plugin
Created By MrDude
*/

#ifndef FTP_H
#define FTP_H

#include <switch.h>

#ifdef __cplusplus
extern "C" {
#endif

// FTP server control functions
bool ftp_init(void);
void ftp_start(PadState* pad);        // Changed
void ftp_stop(PadState* pad);         // Changed
void ftp_cleanup(PadState* pad);  // Add PadState* parameter
bool ftp_is_running(void);
void ftp_update(void);
bool user_connected(void);

// FTP settings (stored in sdmc:/switch/shaderfun/ftp_config.txt), used by the on-screen keyboard page in the library
int ftp_get_port(void);
const char* ftp_get_username(void);
const char* ftp_get_password(void);
// Each setter validates, applies and saves straight away. Returns false when the value is not acceptable.
// Username/password apply to the next login; a new port applies the next time the server is started.
bool ftp_set_port(int port);
bool ftp_set_username(const char* name);
bool ftp_set_password(const char* pass);

#ifdef __cplusplus
}
#endif

#endif // FTP_H