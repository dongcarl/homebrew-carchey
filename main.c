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

#include <crt_externs.h>
#include <dirent.h>
#include <err.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <uuid/uuid.h>


#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFDateFormatter.h>
#include <CoreFoundation/CFString.h>
#include <CoreServices/CoreServices.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

#define OPTIONS "c"

#define ICON_STRING_LINE_1	"                 ###                  "
#define ICON_STRING_LINE_2	"               ####                   "
#define ICON_STRING_LINE_3	"               ###                    "
#define ICON_STRING_LINE_4	"       #######    #######             "
#define ICON_STRING_LINE_5	"     ######################           "
#define ICON_STRING_LINE_6	"    #####################             "
#define ICON_STRING_LINE_7	"    ####################              "
#define ICON_STRING_LINE_8	"    ####################              "
#define ICON_STRING_LINE_9	"    #####################             "
#define ICON_STRING_LINE_10	"     ######################           "
#define ICON_STRING_LINE_11	"      ####################            "
#define ICON_STRING_LINE_12	"        ################              "
#define ICON_STRING_LINE_13	"         ####     #####               "

#define USER_PREFIX		"User: "
#define HOSTNAME_PREFIX		"Hostname: "
#define DISTRO_PREFIX		"Distro: "
#define KERNEL_PREFIX		"Kernel: "
#define UPTIME_PREFIX		"Uptime: "
#define SHELL_PREFIX		"Shell: "
#define TERMINAL_PREFIX		"Terminal: "
#define PACKAGES_PREFIX		"Packages: "
#define CPU_PREFIX		"CPU: "
#define MEMORY_PREFIX		"Memory: "
#define DISK_PREFIX		"Disk: "
#define BATTERY_PREFIX		"Battery: "

#define USER_BUFF_SIZE 		twidth - 44 + 1
#define HOSTNAME_BUFF_SIZE 	twidth - 48 + 1
#define DISTRO_BUFF_SIZE 	twidth - 46 + 1
#define KERNEL_BUFF_SIZE 	twidth - 46 + 1
#define UPTIME_BUFF_SIZE 	twidth - 46 + 1
#define SHELL_BUFF_SIZE         twidth - 45 + 1
#define TERMINAL_BUFF_SIZE      twidth - 48 + 1
#define PACKAGES_BUFF_SIZE      twidth - 48 + 1
#define CPU_BUFF_SIZE 		twidth - 43 + 1
#define MEMORY_BUFF_SIZE        twidth - 46 + 1
#define DISK_BUFF_SIZE 		twidth - 44 + 1
#define BATTERY_BUFF_SIZE       twidth - 47 + 1

static int cflag;
static unsigned short twidth;

__attribute__((noreturn))
static inline void
usage(void)
{
    (void)fprintf(stderr, "usage: archey [-%s]\n", OPTIONS);
    exit(EXIT_FAILURE);
}

static inline void
get_effective_uid(char *buff)
{
    struct passwd *pw = NULL;
    if ((pw = getpwuid(geteuid()))) {
	strlcpy(buff, pw->pw_name, USER_BUFF_SIZE);
    }
    else {
	perror("ERROR: carchey could not get username");
	exit(EXIT_FAILURE);
    }
}

static inline void
get_hostname(char *buff)
{
    if (gethostname(buff, HOSTNAME_BUFF_SIZE)) {
	perror("ERROR: carchey could not get hostname");
	exit(EXIT_FAILURE);
    }

    // trim off domain information like in `hostname -s'
    char *p = strchr(buff, '.');
    if (p != NULL)
	*p = '\0';
}

static inline void
get_uname(char *buff)
{
    int mib[2] = { CTL_KERN, KERN_OSTYPE };
    size_t len = KERNEL_BUFF_SIZE;

    if (sysctl(mib, 2, buff, &len, NULL, 0)) {
	perror("ERROR: carchey could not get username");
	exit(EXIT_FAILURE);
    }
}

static inline void
get_distro(char *buff)
{
    SInt32 majorVersion, minorVersion, bugFixVersion;
    SInt16 err = 0;

    /*
     * Justification for using deprecated Gestalt:
     *     https://twitter.com/Catfish_Man/status/373277120408997889
     */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if ((err = Gestalt(gestaltSystemVersionMajor, &majorVersion))
	|| (err = Gestalt(gestaltSystemVersionMinor, &minorVersion))
	|| (err = Gestalt(gestaltSystemVersionBugFix, &bugFixVersion))) {
	perror("ERROR: carchey could not get distro");
	exit(EXIT_FAILURE);
    }
#pragma clang diagnostic pop
	
    snprintf(buff, DISTRO_BUFF_SIZE, "OS X %d.%d.%d", majorVersion, minorVersion, bugFixVersion);
}

static inline void
get_uptime(char *buff)
{
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };
    struct timeval boottime;
    size_t size = sizeof(boottime);

    if (sysctl(mib, 2, &boottime, &size, NULL, 0) != -1 && boottime.tv_sec != 0) {
	time_t now;
	(void)time(&now);

	time_t uptime = now - boottime.tv_sec;
	
	if (uptime > 60)
	    uptime += 30;
	long days = uptime / 86400l;

	if (days > 0) {
	    (void)snprintf(buff, UPTIME_BUFF_SIZE,"%ld day%s", days, days > 1 ? "s" : "");
	}
	else {
	    long hrs = uptime % 86400 / 3600l;
	    long mins = uptime % 3600 / 60l;

	    if (hrs > 0 && mins > 0)
	        (void)snprintf(buff, UPTIME_BUFF_SIZE, "%2ld:%02ld", hrs, mins);
	    else if (hrs > 0)
	        (void)snprintf(buff, UPTIME_BUFF_SIZE, "%ld hr%s", hrs, hrs > 1 ? "s" : "");
	    else if (mins > 0)
	        (void)snprintf(buff, UPTIME_BUFF_SIZE, "%ld min%s", mins, mins > 1 ? "s" : "");
	    else {
		long secs = uptime % 60;
	        (void)snprintf(buff, UPTIME_BUFF_SIZE, "%ld sec%s", secs, secs > 1 ? "s" : "");
	    }
	}
    }
    else {
	perror("ERROR: carchey could not get uptime");
	exit(EXIT_FAILURE);
    }
}

static inline void
get_terminal(char *buff)
{  
    char *term = getenv("TERM");
    char *term_program = getenv("TERM_PROGRAM");

    if (!*term || !*term_program) {
	perror("ERROR: carchey could not get terminal type");
	exit(EXIT_FAILURE);
    }

    // takes care of Terminal.app which sets $TERM_PROGRAM to "Apple_Terminal"
    //     "Apple Terminal" looks much nicer doesn't it? Fork if you don't agree.
    char *p = strchr(term_program, '_');
    if (p != NULL)
	*p = ' ';

    (void)snprintf(buff, TERMINAL_BUFF_SIZE, "%s %s", term, term_program);
}

static inline void
get_cpu(char *buff)
{
    int mib[3];
    size_t mib_len = 3;
    sysctlnametomib("machdep.cpu.brand_string", mib, &mib_len);
    
    size_t len;
    sysctl(mib, 3, NULL, &len, NULL, 0);
    char *p = malloc(len);
    if (sysctl(mib, 3, p, &len, NULL, 0)) {
	perror("ERROR: carchey could not get CPU model");
	exit(EXIT_FAILURE);
    }
    
    //fuck the police, but not really because of the social contract
    char *tm, *r;
    if ((tm = strstr(p, "(TM)"))) {
	(void)memmove(tm, tm + 4, len);
	len -= 4;
    }
    
    if ((r = strstr(p, "(R)"))) {
	(void)memmove(r, r + 3, len);
    }

    (void)strlcpy(buff, p, CPU_BUFF_SIZE);
    free(p);
}

static inline void
get_mem(char *buff)
{
    int mib[2] = { CTL_HW, HW_MEMSIZE };
    int64_t size = 0;
    size_t len = sizeof(size);

    if (sysctl(mib, 2, &size, &len, NULL, 0)) {
	perror("ERROR: carchey could not get memory capacity");
	exit(EXIT_FAILURE);
    }
	
    (void)snprintf(buff, MEMORY_BUFF_SIZE, "%" PRId64 " GiB", size >> 30);
}

static inline void
get_disk(char *buff)
{
    struct statfs res;
    if (statfs("/", &res)) {
	perror("ERROR: carchey could not get disk usage");
	exit(EXIT_FAILURE);
    }
    unsigned long long percentFree = 100 - ((res.f_bfree * 100) / res.f_blocks);
    (void)snprintf(buff, DISK_BUFF_SIZE,"%llu%%", percentFree);
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

static inline void
get_battery(char *buff, CFDictionaryRef battery_dict)
{
    if (CFDictionaryGetValue(battery_dict, CFSTR(kIOPSIsChargedKey)) == kCFBooleanTrue) {
	(void)snprintf(buff, BATTERY_BUFF_SIZE,"100%%");
    }
    else {
	CFNumberRef current_capacity = CFDictionaryGetValue(battery_dict, CFSTR(kIOPSCurrentCapacityKey));
	CFNumberRef max_capacity = CFDictionaryGetValue(battery_dict, CFSTR(kIOPSMaxCapacityKey));
	if (current_capacity != NULL && max_capacity != NULL) {
	    int currCap;
	    int currMax;
	    CFNumberGetValue(current_capacity, kCFNumberIntType, &currCap);
	    CFNumberGetValue(max_capacity, kCFNumberIntType, &currMax);
	    (void)snprintf(buff, BATTERY_BUFF_SIZE, "%d%%", (currCap * 100) / currMax);
	}
    }
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

static inline void
get_brew(char *buff)
{
    // HFS+ has (2^32) - 1 for the maximum number of files, same as UINT_MAX on OS X
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
    (void)snprintf(buff, PACKAGES_BUFF_SIZE, "%u", file_count);
}

int
main (int argc, char ** argv)
{    
    int ch;
    

    while ((ch = getopt(argc, argv, OPTIONS)) != -1) {
	switch (ch) {
	case 'c':
	    cflag = 1;
	    break;
	case '?':
	default:
	    usage();
	}
    }

    char *KRED = "\x1B[31m";
    char *KGRN = "\x1B[32m";
    char *KYEL = "\x1B[33m";
    char *KBLU = "\x1B[34m";
    char *KMAG = "\x1B[35m";
    char *KCYN = "\x1B[36m";
    char *RESET = "\033[0m";

    if (cflag) {
	KRED = KGRN = KYEL = KBLU = KMAG = KCYN = RESET = "";
    }

    struct ttysize ts;
    ioctl(0, TIOCGSIZE, &ts);
    twidth = ts.ts_cols;

    if (HOSTNAME_BUFF_SIZE < 2) {
	perror("ERROR: terminal width too short for carchey to print anything useful");
	exit(EXIT_FAILURE);
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla"
    
    char user_buff[USER_BUFF_SIZE];
    get_effective_uid(user_buff);
    
    char hostname_buff[HOSTNAME_BUFF_SIZE];
    get_hostname(hostname_buff);

    char distro_buff[DISTRO_BUFF_SIZE];
    get_distro(distro_buff);

    char kernel_buff[KERNEL_BUFF_SIZE];
    get_uname(kernel_buff);

    char uptime_buff[UPTIME_BUFF_SIZE];
    get_uptime(uptime_buff);

    char shell_buff[SHELL_BUFF_SIZE];
    strlcpy(shell_buff, getenv("SHELL"), SHELL_BUFF_SIZE);

    char terminal_buff[TERMINAL_BUFF_SIZE];
    get_terminal(terminal_buff);
     
    char packages_buff[PACKAGES_BUFF_SIZE];
    get_brew(packages_buff);

    char cpu_buff[CPU_BUFF_SIZE];
    get_cpu(cpu_buff);

    char memory_buff[MEMORY_BUFF_SIZE];
    get_mem(memory_buff);

    char disk_buff[DISK_BUFF_SIZE];
    get_disk(disk_buff);

    char battery_buff[BATTERY_BUFF_SIZE];
    CFDictionaryRef internal_battery_dict = get_internal_battery();
    if (internal_battery_dict != NULL)
	get_battery(battery_buff, internal_battery_dict);

#pragma clang diagnostic pop
    
    printf("\n");
    printf("%s%s%s%s%s%s\n", KGRN, ICON_STRING_LINE_1, KCYN, USER_PREFIX, RESET, user_buff);
    printf("%s%s%s%s%s%s\n", KGRN, ICON_STRING_LINE_2, KCYN, HOSTNAME_PREFIX, RESET, hostname_buff);
    printf("%s%s%s%s%s%s\n", KGRN, ICON_STRING_LINE_3, KCYN, DISTRO_PREFIX, RESET, distro_buff);
    printf("%s%s%s%s%s%s\n", KGRN, ICON_STRING_LINE_4, KCYN, KERNEL_PREFIX, RESET, kernel_buff);
    printf("%s%s%s%s%s%s\n", KYEL, ICON_STRING_LINE_5, KCYN, UPTIME_PREFIX, RESET, uptime_buff);
    printf("%s%s%s%s%s%s\n", KYEL, ICON_STRING_LINE_6, KCYN, SHELL_PREFIX, RESET, shell_buff);
    printf("%s%s%s%s%s%s\n", KRED, ICON_STRING_LINE_7, KCYN, TERMINAL_PREFIX, RESET, terminal_buff);
    printf("%s%s%s%s%s%s\n", KRED, ICON_STRING_LINE_8, KCYN, PACKAGES_PREFIX, RESET, packages_buff);
    printf("%s%s%s%s%s%s\n", KRED, ICON_STRING_LINE_9, KCYN, CPU_PREFIX, RESET, cpu_buff);
    printf("%s%s%s%s%s%s\n", KMAG, ICON_STRING_LINE_10, KCYN, MEMORY_PREFIX, RESET, memory_buff);
    printf("%s%s%s%s%s%s\n", KMAG, ICON_STRING_LINE_11, KCYN, DISK_PREFIX, RESET, disk_buff);
    printf("%s%s", KBLU, ICON_STRING_LINE_12); 
    if (internal_battery_dict != NULL)
	printf("%s%s%s%s", KCYN, BATTERY_PREFIX, RESET, battery_buff);
    printf("\n%s%s%s\n\n\n", KBLU, ICON_STRING_LINE_13, RESET);
}
