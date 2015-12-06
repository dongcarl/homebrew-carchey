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
#include <sys/stat.h>

//#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
//#define KWHT  "\x1B[37m"
#define RESET "\033[0m"

#define OPTIONS "co"


static int cflag, oflag;

__attribute__((noreturn))
static inline void
usage(void)
{
	(void)fprintf(stderr, "usage: archey [-%s]\n", OPTIONS);
	exit(1);
}


static inline char *
get_effective_uid(void)
{
	struct passwd *pw = NULL;
	if ((pw = getpwuid(geteuid()))) {
		return pw->pw_name;
	}
	else {
		(void)fprintf(stderr, "ERROR: could not get username");
		exit(1);
	}
}

#define MAXHOSTNAMELEN 256/* max hostname size */

static inline char *
get_hostname(char *buff)
{
	char *p;
  
	if (gethostname(buff, MAXHOSTNAMELEN)) {
		(void)fprintf(stderr, "ERROR: could not get hostname");
		exit(1);
	}

	// trim off domain information like in `hostname -s'
	p = strchr(buff, '.');
	if (p != NULL) {
		*p = '\0';
	}
  
	return buff;
}

static inline char *
get_uname(char *buff)
{
	int mib[2] = { CTL_KERN, KERN_OSTYPE };
	size_t len = _SYS_NAMELEN;

	if (sysctl(mib, 2, buff, &len, NULL, 0)) {
		(void)fprintf(stderr, "ERROR: could not get username");
		exit(1);
	}
    return buff;
}

#define OS_X_VERSION_BUFFER_SIZE 256

static inline char *
get_OS_X_version(char *buff)
{
	SInt32 majorVersion, minorVersion, bugFixVersion;

	/*
	 * Justification for using deprecated Gestalt:
	 *     https://twitter.com/Catfish_Man/status/373277120408997889
	 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	Gestalt(gestaltSystemVersionMajor, &majorVersion);
	Gestalt(gestaltSystemVersionMinor, &minorVersion);
	Gestalt(gestaltSystemVersionBugFix, &bugFixVersion);
#pragma clang diagnostic pop
	
	(void)sprintf(buff, "OS X %d.%d.%d", majorVersion, minorVersion, bugFixVersion);
	return buff;
}

static inline char *
get_uptime(char *buff)
{
	int mib[2] = { CTL_KERN, KERN_BOOTTIME };
	struct timeval boottime;
	size_t size = sizeof(boottime);

	if (sysctl(mib, 2, &boottime, &size, NULL, 0) != -1 && boottime.tv_sec != 0)
	{
		time_t now;
		(void)time(&now);

		time_t uptime = now - boottime.tv_sec;
	
		if (uptime > 60)
			uptime += 30;
	    long days = uptime / 86400l;

		if (days > 0) {
			(void)sprintf(buff, "%ld day%s", days, days > 1 ? "s" : "");
		}
		else {
			long hrs = uptime % 86400 / 3600l;
			long mins = uptime % 3600 / 60l;

			if (hrs > 0 && mins > 0)
				(void)sprintf(buff, "%2ld:%02ld", hrs, mins);
			else if (hrs > 0)
				(void)sprintf(buff, "%ld hr%s", hrs, hrs > 1 ? "s" : "");
			else if (mins > 0)
				(void)sprintf(buff, "%ld min%s", mins, mins > 1 ? "s" : "");
			else {
				long secs = uptime % 60;
				(void)sprintf(buff, "%ld sec%s", secs, secs > 1 ? "s" : "");
			}
		
		}
	}
	else {
		(void)fprintf(stderr, "ERROR: could not get uptime");
		exit(1);
	}
	return buff;
}

static inline char *
get_terminal(char *buff)
{
	char* p;
  
	char *term = getenv("TERM");
	char *term_program = getenv("TERM_PROGRAM");

	// takes care of Terminal.app which sets $TERM_PROGRAM to "Apple_Terminal"
	//     "Apple Terminal" looks much nicer doesn't it?
	p = strchr(term_program, '_');
	if (p != NULL) {
		*p = ' ';
	}

	(void)sprintf(buff, "%s %s", term, term_program);
	return buff;
}

#define CPU_MAX_LEN 256

static inline char *
get_cpu(char *buff)
{
  size_t len = CPU_MAX_LEN;
  if (sysctlbyname("machdep.cpu.brand_string", buff, &len, 0, 0)) {
	  (void)fprintf(stderr, "ERROR: could not get CPU model");
	  exit(1);
  }
  
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
	int mib[2] = { CTL_HW, HW_MEMSIZE };
	int64_t size = 0;
	size_t len = sizeof(size);

	if (sysctl(mib, 2, &size, &len, NULL, 0)) {
		(void)fprintf(stderr, "ERROR: could not get memory capacity");
		exit(1);
	}
	
	(void)sprintf(buff, "%" PRId64 " GiB", size >> 30);
	return buff;
}

static inline char *
get_disk(char *buff)
{
	struct statfs res;
	statfs("/", &res);
	unsigned long long percentFree = 100 - ((res.f_bfree * 100) / res.f_blocks);
	(void)sprintf(buff, "%llu%%", percentFree);
	return buff;
}

static inline int 
is_internal_battery(CFDictionaryRef current_dict)
{
	CFStringRef type = CFDictionaryGetValue(current_dict, CFSTR(kIOPSTypeKey));
	return type != NULL && CFStringCompare(type, CFSTR(kIOPSInternalBatteryType), 0) == kCFCompareEqualTo;
}

static inline CFDictionaryRef 
get_internal_battery()
{
	CFTypeRef power_sources =  IOPSCopyPowerSourcesInfo();
	CFArrayRef power_sources_list = IOPSCopyPowerSourcesList(power_sources);
	for (CFIndex i = 0; i < CFArrayGetCount(power_sources_list); i++) {
		CFDictionaryRef current_dict = IOPSGetPowerSourceDescription(power_sources, CFArrayGetValueAtIndex(power_sources_list, i));
		if (is_internal_battery(current_dict))
			return current_dict;
	}
	return NULL;
}

static inline char *
get_battery(char *buff, CFDictionaryRef battery_dict)
{
	if (CFDictionaryGetValue(battery_dict, CFSTR(kIOPSIsChargedKey)) == kCFBooleanTrue) {
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

static int
is_there(char *candidate)
{
	struct stat fin;
	int result = 0;

	/* XXX work around access(2) false positives for superuser */
	if (access(candidate, X_OK) == 0 &&
		stat(candidate, &fin) == 0 &&
		S_ISREG(fin.st_mode) &&
		(getuid() != 0 ||
		 (fin.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0)) {
		result = 1;
	}
	return result;
}

static inline void
get_brew_path(char *buff)
{
	char *path;
	if ((path = getenv("PATH")) == NULL)
		exit(EXIT_FAILURE);
	char *filename = "brew";
	const char *d;

	while ((d = strsep(&path, ":")) != NULL) {
		if (*d == '\0')
			d = ".";
		if (snprintf(buff, PATH_MAX, "%s/%s", d, filename) >= PATH_MAX)
			continue;
		if(is_there(buff))
			break;
	}
}

static inline void
get_brew_cellar_from_brew_path(char *buff)
{
	char *p = strrchr(buff, '/');
	if (p != NULL)
		*p = '\0';

	p = strrchr(buff, '/');
	if (p != NULL)
		*(p+1) = '\0'; //preserve the last '/'

	strcat(buff, "Cellar");
}

static inline void
get_brew_cellar_path(char *buff)
{
	get_brew_path(buff);
	get_brew_cellar_from_brew_path(buff);
}

static inline char *
get_brew(char *buff)
{
	// HFS+ has (2^32) - 1 for the maximum number of files, same as UINT_MAX
	unsigned int file_count = 0;
	DIR *dirp;
	struct dirent *entry;
	char brew_path[PATH_MAX];

	get_brew_cellar_path(brew_path);
	dirp = opendir(brew_path);
	while ((entry = readdir(dirp)) != NULL) {
		if (strcmp(entry->d_name, ".") != 0
			&& strcmp(entry->d_name, "..") != 0) {
			file_count++;
		}
	}
	closedir(dirp);
	sprintf(buff, "%u", file_count);
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
	char cpu_buff[CPU_MAX_LEN];
	char mem_buff[sizeof(int64_t)];
	char disk_buff[4];
	char battery_buff[4];
	char brew_buff[32];
	CFDictionaryRef internal_battery_dict = get_internal_battery();
  
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
	printf(KGRN "               ###                    " KCYN "Distro: " RESET);       printf("%s\n", get_OS_X_version(version_buff));
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
