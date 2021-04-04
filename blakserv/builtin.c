// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
* builtin.c
*

  This module adds certain hard coded accounts if the accounts cannot
  be read in from the file.
  
*/

#include "blakserv.h"

typedef struct
{
	char *name;
	char *password;
	char *email;
	int type;
} bi_account;

bi_account bi_accounts[] =
{
	{ "admin",  "pass", "None", ACCOUNT_ADMIN},
};

enum
{
	NUM_BUILTIN = sizeof(bi_accounts)/sizeof(bi_account)
};

void CreateBuiltInAccounts(void)
{
	int account_id;

	for (int i = 0; i < NUM_BUILTIN; ++i)
	{
		account_node *a;

		a = GetAccountByName(bi_accounts[i].name);
		if (a != NULL)
			account_id = a->account_id;
		else
			CreateAccount(bi_accounts[i].name, bi_accounts[i].password,
				bi_accounts[i].email, bi_accounts[i].type, &account_id);

		/* make a character for this account */
		switch (bi_accounts[i].type)
		{
		case ACCOUNT_NORMAL :
         CreateNewUser(account_id, USER_CLASS);
			break;
		case ACCOUNT_DM : 
         CreateNewUser(account_id, DM_CLASS);
			break;
		case ACCOUNT_ADMIN :
#if 1
			if (i < 2)
            CreateNewUser(account_id, CREATOR_CLASS);
			else
#endif
            CreateNewUser(account_id, ADMIN_CLASS);
			break;
		}
	}
}
