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

#ifdef __cplusplus
}
#endif

#endif // FTP_H