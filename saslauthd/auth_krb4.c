/* MODULE: auth_krb4 */

/* COPYRIGHT
 * Copyright (c) 1997 Messaging Direct Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY MESSAGING DIRECT LTD. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL MESSAGING DIRECT LTD. OR
 * ITS EMPLOYEES OR AGENTS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * END COPYRIGHT */

#ifdef __GNUC__
#ident "$Id: auth_krb4.c,v 1.4 2001/12/04 02:06:54 rjs3 Exp $"
#endif

/* PUBLIC DEPENDENCIES */
#include <unistd.h>
#include "mechanisms.h"

#ifdef AUTH_KRB4
# include <krb.h>
# include <des.h>
#endif /* AUTH_KRB4 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include "auth_krb4.h"

#ifdef DEADCODE
extern int swap_bytes;			/* from libkrb.a   */
#endif /* DEADCODE */
/* END PUBLIC DEPENDENCIES */

/* PRIVATE DEPENDENCIES */
#ifdef AUTH_KRB4
static char *tf_dir;			/* Ticket directory pathname    */
#endif /* AUTH_KRB4 */
/* END PRIVATE DEPENDENCIES */

#define TF_DIR "/.tf"			/* Private tickets directory,   */
					/* relative to mux directory    */

/* FUNCTION: auth_krb4_init */

/* SYNOPSIS
 * Initialize the Kerberos IV authentication environment.
 *
 * krb4 proxy authentication has a side effect of creating a ticket
 * file for the user we are authenticating. We keep these in a private
 * directory so as not to override a system ticket file that may be
 * in use.
 *
 * This function tries to create the directory, and initializes the
 * global variable tf_dir with the pathname of the directory.
 * END SYNOPSIS */

int					/* R: -1 on failure, else 0 */
auth_krb4_init (
  /* PARAMETERS */
  void					/* no parameters */
  /* END PARAMETERS */
  )
{
#ifdef AUTH_KRB4
    /* VARIABLES */
    int rc;				/* return code holder */
    struct stat sb;			/* stat() work area */
    /* END VARIABLES */

    /* Allocate memory arena for the directory pathname. */
    tf_dir = malloc(strlen(PATH_SASLAUTHD_RUNDIR) + strlen(TF_DIR)
		    + ANAME_SZ + 2);
    if (tf_dir == NULL) {
	syslog(LOG_ERR, "auth_krb4_init malloc(tf_dir) failed");
	return -1;
    }
    strcpy(tf_dir, PATH_SASLAUTHD_RUNDIR);
    strcat(tf_dir, TF_DIR);

    /* Check for an existing directory. */
    rc = lstat(tf_dir, &sb);
    if (rc == -1) {
	if (errno == ENOENT) {
	    /* Create the ticket file directory */
	    rc = mkdir(tf_dir, 0700);
	    if (rc == -1) {
		syslog(LOG_ERR, "auth_krb4_init mkdir(%s): %m",
		       tf_dir);
		free(tf_dir);
		tf_dir = NULL;
		return -1;
	    }
	} else {
	    rc = errno;
	    syslog(LOG_ERR, "auth_krb4_init: %s: %m", tf_dir);
	    free(tf_dir);
	    tf_dir = NULL;
	    return -1;
	}
    }

    /* Make sure it's not a symlink. */
    if (sb.st_mode & S_IFLNK) {
        syslog(LOG_ERR, "auth_krb4_init: %s: is a symbolic link", tf_dir);
	free(tf_dir);
	tf_dir = NULL;
	return -1;
    }
    
    strcat(tf_dir, "/");

    return 0;
#else /* ! AUTH_KRB4 */
    return -1;
#endif /* ! AUTH_KRB4 */
}

/* END FUNCTION: auth_krb4_init */

/* FUNCTION: auth_krb4 */

/* SYNOPSIS
 * Authenticate against Kerberos IV.
 * END SYNOPSIS */

#ifdef AUTH_KRB4

char *					/* R: allocated response string */
auth_krb4 (
  /* PARAMETERS */
  const char *login,			/* I: plaintext authenticator */
  const char *password,			/* I: plaintext password */
  const char *service __attribute__((unused)),
  const char *realm_in __attribute__((unused))
  /* END PARAMETERS */
  )
{
    /* VARIABLES */
    char aname[ANAME_SZ];		/* Kerberos principal */
    char inst[INST_SZ];			/* Kerberos instance (default: imap) */
    char realm[REALM_SZ];		/* Kerberos realm to authenticate in */
    int rc;				/* return code */
    static char *tf_name;		/* Ticket file name */
    /* END VARIABLES */

    /*
     * Make sure we have a password. If this is NULL the call
     * to krb_get_pw_in_tkt below would try to prompt for
     * one interactively.
     */
    if (password == NULL) {
	syslog(LOG_ERR, "auth_krb4: NULL password?");
	return strdup("NO saslauthd internal error");
    }
    /*
     * Use our own private ticket directory. Name the ticket
     * files after the login name.
     *
     * NOTE: these ticket files are not used by us. The are created
     * as a side-effect of calling krb_get_pw_in_tkt.
     */
    tf_name = malloc(strlen(tf_dir) + strlen(login) + 2);
    if (tf_name == NULL) {
	syslog(LOG_ERR, "auth_krb4: malloc(tf_name) failed");
	return strdup("NO saslauthd internal error");

    }
    strcpy(tf_name, tf_dir);
    strcat(tf_name, login);
    krb_set_tkt_string(tf_name);
    
    strncpy(aname, login, ANAME_SZ-1);
    aname[ANAME_SZ-1] = '\0';
    rc = krb_get_lrealm(realm, 1);
    if (rc) {
	syslog(LOG_ERR, "auth_krb4: krb_get_lrealm: %s",
	       krb_get_err_text(rc));
	return strdup("NO saslauthd internal error");
    }
#ifdef NOTYET   
    strcpy(inst, "imap");	/* XXX need to be able to specify this */
    rc = krb_get_pw_in_tkt(aname, inst, realm,
			   KRB_TICKET_GRANTING_TICKET,
			   realm, 1, password);
    if (rc == 0) {
	return strdup("OK");
    }
    if (rc == INTK_BADPW) {
	return strdup("NO");			/* Bad username or password */
    }
#endif    
    /* Try again with null instance */
    inst[0] = '\0';
    rc = krb_get_pw_in_tkt(aname, inst, realm,
			   KRB_TICKET_GRANTING_TICKET,
			   realm, 1, password);
    unlink(tf_name);
    if (rc == 0) {
	return strdup("OK");
    }
    if (rc == INTK_BADPW || rc == KDC_PR_UNKNOWN) {
	return strdup("NO");
    }
    syslog(LOG_ERR, "ERROR: auth_krb4: krb_get_pw_in_tkt: %s",
	   krb_get_err_text(rc));
    return strdup("NO saslauthd internal error");
}

#else /* ! AUTH_KRB4 */

char *
auth_krb4 (
  const char *login __attribute__((unused)),
  const char *password __attribute__((unused)),
  const char *service __attribute__((unused)),
  const char *realm __attribute__((unused))
  )
{
    return NULL;
}

#endif /* ! AUTH_KRB4 */

/* END FUNCTION: auth_krb4 */

/* END MODULE: auth_krb4 */