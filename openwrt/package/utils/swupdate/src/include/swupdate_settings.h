/*
 * (C) Copyright 2016
 * Stefano Babic, DENX Software Engineering, sbabic@denx.de.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */


#ifndef _SWUPDATE_SETTINGS_H
#define _SWUPDATE_SETTINGS_H

#ifdef CONFIG_LIBCONFIG
int read_module_settings(const char *filename, const char *module, settings_callback fcn, void *data);
int read_settings_user_id(const char *filename, const char *module, uid_t *userid, gid_t *groupid);
#else
#include <unistd.h>
static inline int read_module_settings(const char __attribute__ ((__unused__))*filename,
		const char __attribute__ ((__unused__)) *module,
		settings_callback __attribute__ ((__unused__)) fcn,
		void __attribute__ ((__unused__)) *data)
{
	return -1;
}

/*
 * Without LIBCONFIG, let run with current user
 */
static inline int read_settings_user_id(const char __attribute__ ((__unused__))*filename,
					const char __attribute__ ((__unused__))*module,
					uid_t *userid, gid_t *groupid)
{
	*userid = getuid();
	*groupid = getgid();

	return 0;
}
#endif

#endif
