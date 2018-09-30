/***************************************************************************
 begin       : Mon Mar 01 2004
 copyright   : (C) 2018 by Martin Preuss
 email       : martin@libchipcard.de

 ***************************************************************************
 * This file is part of the project "AqBanking".                           *
 * Please see toplevel file COPYING of that project for license details.   *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "account_p.h"
#include "banking_l.h"
#include "provider_l.h"
#include <gwenhywfar/debug.h>
#include <gwenhywfar/misc.h>

#include <stdlib.h>
#include <assert.h>
#include <string.h>


GWEN_INHERIT_FUNCTIONS(AB_ACCOUNT)
GWEN_LIST_FUNCTIONS(AB_ACCOUNT, AB_Account)
GWEN_LIST2_FUNCTIONS(AB_ACCOUNT, AB_Account)




AB_ACCOUNT *AB_Account_new(AB_BANKING *ab, AB_PROVIDER *pro){
  AB_ACCOUNT *a;

  assert(ab);
  assert(pro);
  GWEN_NEW_OBJECT(AB_ACCOUNT, a);
  a->usage=1;
  GWEN_INHERIT_INIT(AB_ACCOUNT, a);
  GWEN_LIST_INIT(AB_ACCOUNT, a);
  a->banking=ab;
  a->provider=pro;
  a->backendName=strdup(AB_Provider_GetName(pro));

  a->userIds=GWEN_StringList_new();
  a->selectedUserIds=GWEN_StringList_new();

  return a;
}



int AB_Account_ReadDb(AB_ACCOUNT *a, GWEN_DB_NODE *db){
  GWEN_DB_NODE *dbT;
  const char *s;
  int i;

  assert(a);
  assert(db);

  a->accountType=GWEN_DB_GetIntValue(db, "accountType", 0, 1);
  a->uniqueId=GWEN_DB_GetIntValue(db, "uniqueId", 0, 1);

  free(a->accountNumber);
  s=GWEN_DB_GetCharValue(db, "accountNumber", 0, NULL);
  if (s)
    a->accountNumber=strdup(s);
  else
    a->accountNumber=NULL;

  free(a->subAccountId);
  s=GWEN_DB_GetCharValue(db, "subAccountId", 0, NULL);
  if (s)
    a->subAccountId=strdup(s);
  else
    a->subAccountId=NULL;

  free(a->bankCode);
  s=GWEN_DB_GetCharValue(db, "bankCode", 0, NULL);
  if (s)
    a->bankCode=strdup(s);
  else
    a->bankCode=NULL;

  free(a->accountName);
  s=GWEN_DB_GetCharValue(db, "accountName", 0, NULL);
  if (s)
    a->accountName=strdup(s);
  else
    a->accountName=NULL;

  free(a->bankName);
  s=GWEN_DB_GetCharValue(db, "bankName", 0, NULL);
  if (s)
    a->bankName=strdup(s);
  else
    a->bankName=NULL;

  free(a->iban);
  s=GWEN_DB_GetCharValue(db, "iban", 0, NULL);
  if (s)
    a->iban=strdup(s);
  else
    a->iban=NULL;

  free(a->bic);
  s=GWEN_DB_GetCharValue(db, "bic", 0, NULL);
  if (s)
    a->bic=strdup(s);
  else
    a->bic=NULL;

  free(a->ownerName);
  s=GWEN_DB_GetCharValue(db, "ownerName", 0, NULL);
  if (s)
    a->ownerName=strdup(s);
  else
    a->ownerName=NULL;

  free(a->currency);
  s=GWEN_DB_GetCharValue(db, "currency", 0, "EUR");
  if (s)
    a->currency=strdup(s);
  else
    a->currency=NULL;

  free(a->country);
  s=GWEN_DB_GetCharValue(db, "country", 0, "de");
  if (s)
    a->country=strdup(s);
  else
    a->country=NULL;

  GWEN_StringList_Clear(a->userIds);
  for (i=0; i<100; i++) {
    uint32_t id;
    char numbuf[16];

    id=GWEN_DB_GetIntValue(db, "user", i, 0);
    if (id==0)
      break;
    snprintf(numbuf, sizeof(numbuf)-1, "%u", id);
    GWEN_StringList_AppendString(a->userIds, numbuf, 0, 1);
  }

  GWEN_StringList_Clear(a->selectedUserIds);
  for (i=0; i<100; i++) {
    uint32_t id;
    char numbuf[16];

    id=GWEN_DB_GetIntValue(db, "selectedUser", i, 0);
    if (id==0)
      break;
    snprintf(numbuf, sizeof(numbuf)-1, "%u", id);
    GWEN_StringList_AppendString(a->selectedUserIds, numbuf, 0, 1);
  }
  /* fix problem where there is no userId in the list */
  if (GWEN_StringList_Count(a->userIds)<GWEN_StringList_Count(a->selectedUserIds)) {
    GWEN_STRINGLISTENTRY *se;
  
    se=GWEN_StringList_FirstEntry(a->selectedUserIds);
    while(se) {
      const char *s;
  
      s=GWEN_StringListEntry_Data(se);
      if (s) {
	GWEN_StringList_AppendString(a->userIds, s, 0, 1);
      }

      se=GWEN_StringListEntry_Next(se);
    }
  }

  /* TODO: START: remove this */
  GWEN_DB_Group_free(a->appData);
  dbT=GWEN_DB_GetGroup(db, GWEN_PATH_FLAGS_NAMEMUSTEXIST, "apps");
  if (dbT)
    a->appData=GWEN_DB_Group_dup(dbT);
  else
    a->appData=GWEN_DB_Group_new("apps");

  GWEN_DB_Group_free(a->providerData);
  dbT=GWEN_DB_GetGroup(db, GWEN_PATH_FLAGS_NAMEMUSTEXIST, "provider");
  if (dbT)
    a->providerData=GWEN_DB_Group_dup(dbT);
  else
    a->providerData=GWEN_DB_Group_new("provider");
  /* TODO: END: remove this */

  return 0;
}



const char *AB_Account_GetBackendName(const AB_ACCOUNT *a) {
  assert(a);
  assert(a->usage);
  return a->backendName;
}



const char *AB_Account_GetFirstUserId(const AB_ACCOUNT *a) {
  assert(a);
  assert(a->usage);

  return GWEN_StringList_FirstString(a->userIds);
}



uint32_t AB_Account_GetFirstUserIdAsInt(const AB_ACCOUNT *a) {
  const char *s;

  assert(a);
  assert(a->usage);

  s=GWEN_StringList_FirstString(a->userIds);
  if (s && *s) {
    unsigned long int id;
  
    if (1!=sscanf(s, "%lu", &id)) {
      DBG_ERROR(AQBANKING_LOGDOMAIN, "Invalid id of first account user (%s), SNH!", s);
      return 0;
    }
    return (uint32_t) id;
  }
  return 0;
}



void AB_Account_SetUserId(AB_ACCOUNT *a, uint32_t uid) {
  char numbuf[16];

  assert(a);
  assert(a->usage);

  GWEN_StringList_Clear(a->userIds);

  snprintf(numbuf, sizeof(numbuf)-1, "%u", uid);
  GWEN_StringList_AppendString(a->userIds, numbuf, 0, 1);
}



int AB_Account_HasUserId(const AB_ACCOUNT *a, uint32_t uid) {
  GWEN_STRINGLISTENTRY *se;

  assert(a);
  assert(a->usage);

  se=GWEN_StringList_FirstEntry(a->userIds);
  while(se) {
    const char *s;

    s=GWEN_StringListEntry_Data(se);
    if (s) {
      unsigned int id;

      if (1==sscanf(s, "%u", &id)) {
        if (id==uid)
          return 0;
      }
    }

    se=GWEN_StringListEntry_Next(se);
  }

  DBG_INFO(AQBANKING_LOGDOMAIN, "User with id \"%u\" not found", uid);
  return GWEN_ERROR_NOT_FOUND;
}




int AB_Account_toDb(const AB_ACCOUNT *a, GWEN_DB_NODE *db){
  GWEN_STRINGLISTENTRY *se;
  GWEN_DB_NODE *dbT;

  assert(a);
  assert(a->usage);

  GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
		       "provider", a->backendName);

  GWEN_DB_SetIntValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
		      "uniqueId", a->uniqueId);
  GWEN_DB_SetIntValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
		      "accountType", a->accountType);

  if (a->accountNumber)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			 "accountNumber", a->accountNumber);

  if (a->subAccountId)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
                         "subAccountId", a->subAccountId);

  if (a->bankCode)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			 "bankCode", a->bankCode);
  if (a->accountName)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			 "accountName", a->accountName);
  if (a->bankName)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			 "bankName", a->bankName);
  if (a->iban)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			 "iban", a->iban);
  if (a->bic)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			 "bic", a->bic);
  if (a->ownerName)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			 "ownerName", a->ownerName);
  if (a->currency)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			 "currency", a->currency);
  if (a->country)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			 "country", a->country);

  GWEN_DB_DeleteVar(db, "user");
  se=GWEN_StringList_FirstEntry(a->userIds);
  while(se) {
    const char *s;

    s=GWEN_StringListEntry_Data(se);
    if (s) {
      unsigned int id;

      if (1==sscanf(s, "%u", &id))
	GWEN_DB_SetIntValue(db, GWEN_DB_FLAGS_DEFAULT, "user", id);
    }
    se=GWEN_StringListEntry_Next(se);
  }

  GWEN_DB_DeleteVar(db, "selectedUser");
  se=GWEN_StringList_FirstEntry(a->selectedUserIds);
  while(se) {
    const char *s;

    s=GWEN_StringListEntry_Data(se);
    if (s) {
      unsigned int id;

      if (1==sscanf(s, "%u", &id))
	GWEN_DB_SetIntValue(db, GWEN_DB_FLAGS_DEFAULT, "selectedUser", id);
    }
    se=GWEN_StringListEntry_Next(se);
  }

  if (a->appData) {
    dbT=GWEN_DB_GetGroup(db, GWEN_DB_FLAGS_OVERWRITE_GROUPS, "apps");
    assert(dbT);
    GWEN_DB_AddGroupChildren(dbT, a->appData);
  }

  if (a->providerData) {
    dbT=GWEN_DB_GetGroup(db, GWEN_DB_FLAGS_OVERWRITE_GROUPS, "provider");
    assert(dbT);
    GWEN_DB_AddGroupChildren(dbT, a->providerData);
  }

  return 0;
}



void AB_Account_free(AB_ACCOUNT *a){
  if (a) {
    assert(a->usage);
    if (a->usage==1) {
      DBG_VERBOUS(AQBANKING_LOGDOMAIN, "Destroying AB_ACCOUNT");
      GWEN_LIST_FINI(AB_ACCOUNT, a);
      GWEN_INHERIT_FINI(AB_ACCOUNT, a);
      free(a->backendName);

      free(a->subAccountId);
      free(a->accountNumber);
      free(a->bankCode);
      free(a->accountName);
      free(a->bankName);
      free(a->iban);
      free(a->bic);
      free(a->ownerName);
      free(a->currency);
      free(a->country);
      free(a->dbId);

      GWEN_StringList_free(a->userIds);
      GWEN_StringList_free(a->selectedUserIds);


      GWEN_DB_Group_free(a->appData);
      GWEN_DB_Group_free(a->providerData);

      a->usage=0;
      GWEN_FREE_OBJECT(a);
    }
    else
      a->usage--;
  }
}



void AB_Account_Attach(AB_ACCOUNT *a){
  assert(a);
  a->usage++;
}



uint32_t AB_Account_GetUniqueId(const AB_ACCOUNT *a){
  assert(a);
  assert(a->usage);
  return a->uniqueId;
}



void AB_Account_SetUniqueId(AB_ACCOUNT *a, uint32_t id){
  assert(a);
  assert(a->usage);
  a->uniqueId=id;
}



AB_ACCOUNT_TYPE AB_Account_GetAccountType(const AB_ACCOUNT *a){
  assert(a);
  assert(a->usage);
  return a->accountType;
}



void AB_Account_SetAccountType(AB_ACCOUNT *a, AB_ACCOUNT_TYPE t){
  assert(a);
  assert(a->usage);
  a->accountType=t;
}



AB_PROVIDER *AB_Account_GetProvider(const AB_ACCOUNT *a) {
  assert(a);
  assert(a->usage);
  return a->provider;
}



const char *AB_Account_GetAccountNumber(const AB_ACCOUNT *a){
  assert(a);
  assert(a->usage);
  return a->accountNumber;
}



void AB_Account_SetAccountNumber(AB_ACCOUNT *a, const char *s){
  assert(a);
  assert(a->usage);

  free(a->accountNumber);
  if (s)
    a->accountNumber=strdup(s);
  else
    a->accountNumber=NULL;
}



const char *AB_Account_GetSubAccountId(const AB_ACCOUNT *a){
  assert(a);
  assert(a->usage);
  return a->subAccountId;
}



void AB_Account_SetSubAccountId(AB_ACCOUNT *a, const char *s){
  assert(a);
  assert(a->usage);

  free(a->subAccountId);
  if (s)
    a->subAccountId=strdup(s);
  else
    a->subAccountId=NULL;
}



const char *AB_Account_GetBankCode(const AB_ACCOUNT *a){
  assert(a);
  assert(a->usage);

  return a->bankCode;
}



void AB_Account_SetBankCode(AB_ACCOUNT *a, const char *s){
  assert(a);
  assert(a->usage);

  free(a->bankCode);
  if (s)
    a->bankCode=strdup(s);
  else
    a->bankCode=NULL;

}



const char *AB_Account_GetAccountName(const AB_ACCOUNT *a){
  assert(a);
  assert(a->usage);

  return a->accountName;
}



void AB_Account_SetAccountName(AB_ACCOUNT *a, const char *s){
  assert(a);
  assert(a->usage);

  free(a->accountName);
  if (s)
    a->accountName=strdup(s);
  else
    a->accountName=NULL;

}



const char *AB_Account_GetBankName(const AB_ACCOUNT *a){
  assert(a);
  assert(a->usage);

  return a->bankName;
}



void AB_Account_SetBankName(AB_ACCOUNT *a, const char *s){
  assert(a);
  assert(a->usage);

  free(a->bankName);
  if (s)
    a->bankName=strdup(s);
  else
    a->bankName=NULL;
}



const char *AB_Account_GetIBAN(const AB_ACCOUNT *a){
  assert(a);
  assert(a->usage);

  return a->iban;
}



void AB_Account_SetIBAN(AB_ACCOUNT *a, const char *s){
  assert(a);
  assert(a->usage);

  free(a->iban);
  if (s)
    a->iban=strdup(s);
  else
    a->iban=NULL;
}



const char *AB_Account_GetBIC(const AB_ACCOUNT *a){
  assert(a);
  assert(a->usage);

  return a->bic;
}



void AB_Account_SetBIC(AB_ACCOUNT *a, const char *s){
  assert(a);
  assert(a->usage);

  free(a->bic);
  if (s)
    a->bic=strdup(s);
  else
    a->bic=NULL;
}



const char *AB_Account_GetOwnerName(const AB_ACCOUNT *a){
  assert(a);
  assert(a->usage);

  return a->ownerName;
}



void AB_Account_SetOwnerName(AB_ACCOUNT *a, const char *s){
  assert(a);
  assert(a->usage);

  free(a->ownerName);
  if (s)
    a->ownerName=strdup(s);
  else
    a->ownerName=NULL;
}



AB_BANKING *AB_Account_GetBanking(const AB_ACCOUNT *a){
  assert(a);
  assert(a->usage);
  return a->banking;
}



const char *AB_Account_GetCurrency(const AB_ACCOUNT *a){
  assert(a);
  assert(a->usage);

  return a->currency;
}




void AB_Account_SetCurrency(AB_ACCOUNT *a, const char *s){
  assert(a);
  assert(a->usage);
  assert(s);

  free(a->currency);
  if (s)
    a->currency=strdup(s);
  else
    a->currency=NULL;
}



const char *AB_Account_GetCountry(const AB_ACCOUNT *a){
  assert(a);
  assert(a->usage);

  return a->country;
}



void AB_Account_SetCountry(AB_ACCOUNT *a, const char *s){
  assert(a);
  assert(a->usage);
  assert(s);

  free(a->country);
  if (s)
    a->country=strdup(s);
  else
    a->country=NULL;
}



void AB_Account_SetProvider(AB_ACCOUNT *a, AB_PROVIDER *pro){
  assert(a);
  assert(a->usage);
  assert(pro);
  a->provider=pro;

  free(a->backendName);
  a->backendName=strdup(AB_Provider_GetName(pro));
}



const char *AB_Account_GetDbId(const AB_ACCOUNT *a) {
  assert(a);
  assert(a->usage);

  return a->dbId;
}



void AB_Account_SetDbId(AB_ACCOUNT *a, const char *s) {
  assert(a);
  assert(a->usage);

  free(a->dbId);
  if (s) a->dbId=strdup(s);
  else a->dbId=NULL;
}



AB_ACCOUNT *AB_Account__freeAll_cb(AB_ACCOUNT *a, void *userData) {
  AB_Account_free(a);
  return 0;
}



void AB_Account_List2_FreeAll(AB_ACCOUNT_LIST2 *al){
  AB_Account_List2_ForEach(al, AB_Account__freeAll_cb, 0);
  AB_Account_List2_free(al);
}





AB_ACCOUNT *AB_Account_List2_GetAccountByUniqueId(const AB_ACCOUNT_LIST2 *al, uint32_t aid) {
  AB_ACCOUNT_LIST2_ITERATOR *ait;

  ait=AB_Account_List2_First(al);
  if (ait) {
    AB_ACCOUNT *a;

    a=AB_Account_List2Iterator_Data(ait);
    assert(a);
    while(a) {
      if (aid==AB_Account_GetUniqueId(a)) {
        AB_Account_List2Iterator_free(ait);
        return a;
      }
      a=AB_Account_List2Iterator_Next(ait);
    }
    AB_Account_List2Iterator_free(ait);
  }

  return NULL;
}







