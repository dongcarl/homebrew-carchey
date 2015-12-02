/*-
 * Copyright (c) 1988, 1993
 *The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
  This was hacked up on a drunken night
  Dragons are definitely here
*/

#include <dirent.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFDateFormatter.h>
#include <CoreServices/CoreServices.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <uuid/uuid.h>
#include <crt_externs.h>

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"
#define RESET "\033[0m"


static int cflag, oflag;

static inline void
usage(void)
{
  (void)fprintf(stderr, "%s\n",
		"usage: archey [-co]");
  exit(1);
}

static inline char *
get_effective_uid(void)
{
  struct passwd *pw = NULL;
  if ((pw = getpwuid(geteuid())))
    return pw->pw_name;
  else
    exit(1);
}

#define MAXHOSTNAMELEN 256/* max hostname size */

static inline char *
get_hostname(char *buff)
{
  char *p;
  
  if (gethostname(buff, MAXHOSTNAMELEN))
    err(1, "gethostname");

  p = strchr(buff, '.');
  if (p != NULL)
    *p = '\0';
  
  return buff;
}

static inline char *
get_uname(char *buff)
{
  int mib[2] = {CTL_KERN, KERN_OSTYPE};
  size_t len = _SYS_NAMELEN;

  if (sysctl(mib, 2, buff, &len, NULL, 0) == -1)
    err(1, "get_uname");
  return buff;
}

#define OS_X_VERSION_BUFFER_SIZE 256

static inline char *
get_OS_X_version(char *buff)
{
  SInt32 majorVersion,minorVersion,bugFixVersion;

  Gestalt(gestaltSystemVersionMajor, &majorVersion);
  Gestalt(gestaltSystemVersionMinor, &minorVersion);
  Gestalt(gestaltSystemVersionBugFix, &bugFixVersion);
  sprintf(buff, "OS X %d.%d.%d", majorVersion, minorVersion, bugFixVersion);

  return buff;
}

static inline char *
get_uptime(char *buff)
{
  time_t uptime;
  int days, hrs, mins, secs;
  int mib[2];
  size_t size;
  struct timeval boottime;
  time_t now;

  (void)time(&now);
    
  mib[0] = CTL_KERN;
  mib[1] = KERN_BOOTTIME;
  size = sizeof(boottime);

  if (sysctl(mib, 2, &boottime, &size, NULL, 0) != -1 && boottime.tv_sec != 0)
  {
    uptime = now - boottime.tv_sec;
    if (uptime > 60)
      uptime += 30;
    days = uptime / 86400;
    uptime %= 86400;
    hrs = uptime / 3600;
    uptime %= 3600;
    mins = uptime / 60;
    secs = uptime % 60;
    if (days > 0) {
      (void)sprintf(buff, "%d day%s", days, days > 1 ? "s" : "");
      return buff;
    }
    if (hrs > 0 && mins > 0)
      (void)sprintf(buff, "%2d:%02d", hrs, mins);
    else if (hrs > 0)
      (void)sprintf(buff, "%d hr%s", hrs, hrs > 1 ? "s" : "");
    else if (mins > 0)
      (void)sprintf(buff, "%d min%s", mins, mins > 1 ? "s" : "");
    else
      (void)sprintf(buff, "%d sec%s", secs, secs > 1 ? "s" : "");
  }
  return buff;
}

static inline char *
get_terminal(char *buff) {
  char* p;
  
  char *term = getenv("TERM");
  char *term_program = getenv("TERM_PROGRAM");

  p = strchr(term_program, '_');
  if (p != NULL)
    *p = ' ';

  sprintf(buff, "%s %s", term, term_program);

  return buff;
}

#define CPU_MAX_LEX 256

static inline char *
get_cpu(char *buff)
{
  size_t len = CPU_MAX_LEX;
  if (sysctlbyname("machdep.cpu.brand_string", buff, &len, 0, 0))
    err(1, "get_cpu");

  //fuck the police
  char *tm = strstr(buff, "(TM)");
  memmove(tm, tm + 4, len);

  char *r = strstr(buff, "(R)");
  memmove(r, r + 3, len);

  return buff;
}

static inline char *
get_mem(char *buff)
{
  int mib[2];
  int64_t size = 0;
  size_t len = sizeof(size);

  mib[0] = CTL_HW;
  mib[1] = HW_MEMSIZE;
  if (sysctl(mib, 2, &size, &len, NULL, 0))
    err(1, "get_mem");

  (void)sprintf(buff, "%" PRId64 " GiB", size >> 30);
  return buff;
}

static inline char *
get_disk(char *buff)
{
  struct statfs res;
  statfs("/", &res);
  unsigned short percentFree = 100 - ((res.f_bfree * 100) / res.f_blocks);
  (void)sprintf(buff, "%hu%%", percentFree);
  return buff;
}

static inline int 
isInternalBattery(CFDictionaryRef current_dict) {
  CFStringRef type = CFDictionaryGetValue(current_dict, CFSTR(kIOPSTypeKey));
  return type != NULL && CFStringCompare(type, CFSTR(kIOPSInternalBatteryType), 0) == kCFCompareEqualTo;
}

static inline CFDictionaryRef 
getInternalBattery() {
  CFTypeRef power_sources =  IOPSCopyPowerSourcesInfo();
  CFArrayRef power_sources_list = IOPSCopyPowerSourcesList(power_sources);
  for (CFIndex i = 0; i < CFArrayGetCount(power_sources_list); i++) {
    CFDictionaryRef current_dict = IOPSGetPowerSourceDescription(power_sources, CFArrayGetValueAtIndex(power_sources_list, i));
    if (isInternalBattery(current_dict))
      return current_dict;
  }
  return NULL;
}

static inline char *
get_battery(char *buff, CFDictionaryRef battery_dict) {
  if (CFBooleanGetValue(CFDictionaryGetValue(battery_dict, CFSTR(kIOPSIsChargedKey)))) {
    sprintf(buff, "100%%");
  }
  else {
    CFNumberRef current_capacity = CFDictionaryGetValue(battery_dict, CFSTR(kIOPSCurrentCapacityKey));
    CFNumberRef max_capacity = CFDictionaryGetValue(battery_dict, CFSTR(kIOPSMaxCapacityKey));
    if (current_capacity != NULL && max_capacity != NULL) {
      int currCap;
      int currMax;
      CFNumberGetValue(current_capacity, kCFNumberIntType, &currCap);
      CFNumberGetValue(max_capacity, kCFNumberIntType, &currMax);
      sprintf(buff, "%d%%", (currCap * 100) / currMax);
    }
  }
  return buff;
}

static inline char *
get_brew(char *buff) {
  int file_count = 0;
  DIR *dirp;
  struct dirent *entry;

  dirp = opendir("/usr/local/Cellar");
  while ((entry = readdir(dirp)) != NULL) {
    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
      file_count++;
    }
  }
  closedir(dirp);
  sprintf(buff, "%d", file_count);
  return buff;
}


int
main (int argc, char ** argv)
{
  int ch;
  char hostname_buff[MAXHOSTNAMELEN];
  char uname_buff[_SYS_NAMELEN];
  char version_buff[OS_X_VERSION_BUFFER_SIZE];
  char uptime_buff[512];
  char terminal_buff[64];
  char cpu_buff[CPU_MAX_LEX];
  char mem_buff[sizeof(int64_t)];
  char disk_buff[4];
  char battery_buff[4];
  char os_buff[16];
  char brew_buff[32];
  CFDictionaryRef internal_battery_dict = getInternalBattery();
  
  while ((ch = getopt(argc, argv, "co")) != -1) {
    switch (ch) {
    case 'c':
      cflag = 1;
      break;
    case 'o':
      oflag = 1;
      break;
    case '?':
    default:
      usage();
    }
  }

  printf("\n");
  printf(KGRN "                 ###                  " KCYN "User: " RESET);         printf("%s\n", get_effective_uid());
  printf(KGRN "               ####                   " KCYN "Hostname: " RESET);     printf("%s\n", get_hostname(hostname_buff));
  printf(KGRN "               ###                    " KCYN "Distro: " RESET);       printf("%s\n", get_OS_X_version(os_buff));
  printf(KGRN "       #######    #######             " KCYN "Kernel: " RESET);       printf("%s\n", get_uname(uname_buff));
  printf(KYEL "     ######################           " KCYN "Uptime: " RESET);       printf("%s\n", get_uptime(uptime_buff));
  printf(KYEL "    #####################             " KCYN "Shell: " RESET);        printf("%s\n", getenv("SHELL"));
  printf(KRED "    ####################              " KCYN "Terminal: " RESET);     printf("%s\n", get_terminal(terminal_buff));
  printf(KRED "    ####################              " KCYN "Packages: " RESET);     printf("%s\n", get_brew(brew_buff));
  printf(KRED "    #####################             " KCYN "CPU: " RESET);          printf("%s\n", get_cpu(cpu_buff));
  printf(KMAG "     ######################           " KCYN "Memory: " RESET);       printf("%s\n", get_mem(mem_buff));
  printf(KMAG "      ####################            " KCYN "Disk: " RESET);         printf("%s\n", get_disk(disk_buff));
  printf(KBLU "        ################              "); 
  if (internal_battery_dict != NULL) {
      printf(KCYN "Battery: " RESET);
      printf("%s\n", get_battery(battery_buff, internal_battery_dict));
  }
  else {
  	  printf("\n");
  }
  printf(KBLU "         ####     #####               " RESET);
  printf("\n\n\n");
}
