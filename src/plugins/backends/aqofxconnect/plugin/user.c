/***************************************************************************
 $RCSfile$
                             -------------------
    cvs         : $Id$
    begin       : Mon Mar 01 2004
    copyright   : (C) 2004 by Martin Preuss
    email       : martin@libchipcard.de

 ***************************************************************************
 *          Please see toplevel file COPYING for license details           *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "user_p.h"


#include <gwenhywfar/debug.h>
#include <gwenhywfar/misc.h>

#include <stdlib.h>
#include <assert.h>
#include <string.h>



GWEN_INHERIT(AB_USER, AO_USER);



AO_USER_SERVERTYPE AO_User_ServerType_fromString(const char *s) {
  assert(s);
  if (strcasecmp(s, "http")==0)
    return AO_User_ServerTypeHTTP;
  else if (strcasecmp(s, "https")==0)
    return AO_User_ServerTypeHTTPS;
  return AO_User_ServerTypeUnknown;
}



const char *AO_User_ServerType_toString(AO_USER_SERVERTYPE t) {
  switch(t) {
  case AO_User_ServerTypeHTTP:  return "http";
  case AO_User_ServerTypeHTTPS: return "https";
  default:                      return "unknown";
  }
}



uint32_t AO_User_Flags_fromDb(GWEN_DB_NODE *db, const char *name) {
  int i;
  uint32_t f=0;

  for (i=0; ; i++) {
    const char *s;

    s=GWEN_DB_GetCharValue(db, name, i, 0);
    if (!s)
      break;
    if (strcasecmp(s, "account_list")==0)
      f|=AO_USER_FLAGS_ACCOUNT_LIST;
    else if (strcasecmp(s, "statements")==0)
      f|=AO_USER_FLAGS_STATEMENTS;
    else if (strcasecmp(s, "investment")==0)
      f|=AO_USER_FLAGS_INVESTMENT;
    else if (strcasecmp(s, "billpay")==0)
      f|=AO_USER_FLAGS_BILLPAY;
    else if (strcasecmp(s, "emptyBankId")==0)
      f|=AO_USER_FLAGS_EMPTY_BANKID;
    else if (strcasecmp(s, "emptyFid")==0)
      f|=AO_USER_FLAGS_EMPTY_FID;
    else if (strcasecmp(s, "forceSsl3")==0)
      f|=AO_USER_FLAGS_FORCE_SSL3;
    else if (strcasecmp(s, "sendShortDate")==0)
      f|=AO_USER_FLAGS_SEND_SHORT_DATE;
    else {
      DBG_ERROR(AQOFXCONNECT_LOGDOMAIN,
                "Unknown user flag \"%s\"", s);
    }
  }
  return f;
}



void AO_User_Flags_toDb(GWEN_DB_NODE *db, const char *name,
                        uint32_t f) {
  GWEN_DB_DeleteVar(db, name);
  if (f & AO_USER_FLAGS_ACCOUNT_LIST)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_DEFAULT, name,
                         "account_list");
  if (f & AO_USER_FLAGS_STATEMENTS)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_DEFAULT, name,
                         "statements");
  if (f & AO_USER_FLAGS_INVESTMENT)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_DEFAULT, name,
                         "investment");
  if (f & AO_USER_FLAGS_BILLPAY)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_DEFAULT, name,
                         "billpay");
  if (f & AO_USER_FLAGS_EMPTY_BANKID)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_DEFAULT, name,
			 "emptyBankId");
  if (f & AO_USER_FLAGS_EMPTY_FID)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_DEFAULT, name,
			 "emptyFid");
  if (f & AO_USER_FLAGS_FORCE_SSL3)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_DEFAULT, name,
			 "forceSsl3");
  if (f & AO_USER_FLAGS_SEND_SHORT_DATE)
    GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_DEFAULT, name,
			 "sendShortDate");
}




void AO_User_Extend(AB_USER *u, AB_PROVIDER *pro,
		    AB_PROVIDER_EXTEND_MODE em,
		    GWEN_DB_NODE *db) {
  if (em==AB_ProviderExtendMode_Create ||
      em==AB_ProviderExtendMode_Extend) {
    AO_USER *ue;

    GWEN_NEW_OBJECT(AO_USER, ue);
    GWEN_INHERIT_SETDATA(AB_USER, AO_USER, u, ue, AO_User_FreeData);

    if (em==AB_ProviderExtendMode_Create) {
    }
    else {
      const char *s;

      ue->flags=AO_User_Flags_fromDb(db, "flags");

      free(ue->brokerId);
      s=GWEN_DB_GetCharValue(db, "brokerId", 0, NULL);
      if (s)
	ue->brokerId=strdup(s);
      else
	ue->brokerId=NULL;

      free(ue->org);
      s=GWEN_DB_GetCharValue(db, "org", 0, NULL);
      if (s)
	ue->org=strdup(s);
      else
	ue->org=NULL;

      free(ue->fid);
      s=GWEN_DB_GetCharValue(db, "fid", 0, NULL);
      if (s)
	ue->fid=strdup(s);
      else
	ue->fid=NULL;

      s=GWEN_DB_GetCharValue(db, "serverType", 0, "https");
      ue->serverType=AO_User_ServerType_fromString(s);

      free(ue->serverAddr);
      s=GWEN_DB_GetCharValue(db, "serverAddr", 0, NULL);
      if (s)
	ue->serverAddr=strdup(s);
      else
	ue->serverAddr=NULL;

      ue->serverPort=GWEN_DB_GetIntValue(db, "serverPort", 0, 0);

      free(ue->appId);
      s=GWEN_DB_GetCharValue(db, "appId", 0, NULL);
      if (s)
	ue->appId=strdup(s);
      else
	ue->appId=NULL;

      free(ue->appVer);
      s=GWEN_DB_GetCharValue(db, "appVer", 0, NULL);
      if (s)
	ue->appVer=strdup(s);
      else
	ue->appVer=NULL;

      free(ue->headerVer);
      s=GWEN_DB_GetCharValue(db, "headerVer", 0, NULL);
      if (s)
	ue->headerVer=strdup(s);
      else
	ue->headerVer=NULL;

      free(ue->clientUid);
      s=GWEN_DB_GetCharValue(db, "clientUid", 0, NULL);
      if (s)
	ue->clientUid=strdup(s);
      else
	ue->clientUid=NULL;
    }

  }
  else {
    AO_USER *ue;

    ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
    assert(ue);

    if (em==AB_ProviderExtendMode_Save) {
      AO_User_Flags_toDb(db, "flags", ue->flags);

      if (ue->brokerId)
	GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			     "brokerId", ue->brokerId);

      if (ue->org)
	GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			     "org", ue->org);

      if (ue->fid)
	GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			     "fid", ue->fid);

      GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			   "serverType",
			   AO_User_ServerType_toString(ue->serverType));

      if (ue->serverAddr)
	GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			     "serverAddr", ue->serverAddr);

      GWEN_DB_SetIntValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
                          "serverPort", ue->serverPort);

      if (ue->appId)
	GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			     "appId", ue->appId);

      if (ue->appVer)
	GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			     "appVer", ue->appVer);

      if (ue->headerVer)
	GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			     "headerVer", ue->headerVer);
      if (ue->clientUid)
	GWEN_DB_SetCharValue(db, GWEN_DB_FLAGS_OVERWRITE_VARS,
			     "clientUid", ue->clientUid);
    }
  }
}



void GWENHYWFAR_CB AO_User_FreeData(void *bp, void *p) {
  AO_USER *ue;

  ue=(AO_USER*)p;

  free(ue->brokerId);
  free(ue->org);
  free(ue->fid);
  free(ue->serverAddr);
  free(ue->appId);
  free(ue->appVer);
  free(ue->headerVer);
  free(ue->clientUid);

  GWEN_FREE_OBJECT(ue);
}



const char *AO_User_GetBrokerId(const AB_USER *u) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  return ue->brokerId;
}



void AO_User_SetBrokerId(AB_USER *u, const char *s) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  free(ue->brokerId);
  if (s) ue->brokerId=strdup(s);
  else ue->brokerId=NULL;
}



const char *AO_User_GetOrg(const AB_USER *u) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  return ue->org;
}



void AO_User_SetOrg(AB_USER *u, const char *s) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  free(ue->org);
  if (s) ue->org=strdup(s);
  else ue->org=NULL;
}




const char *AO_User_GetFid(const AB_USER *u) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  return ue->fid;
}



void AO_User_SetFid(AB_USER *u, const char *s) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  free(ue->fid);
  if (s) ue->fid=strdup(s);
  else ue->fid=NULL;
}



AO_USER_SERVERTYPE AO_User_GetServerType(const AB_USER *u) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  return ue->serverType;
}



void AO_User_SetServerType(AB_USER *u, AO_USER_SERVERTYPE t) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  ue->serverType=t;
}



const char *AO_User_GetServerAddr(const AB_USER *u) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  return ue->serverAddr;
}



void AO_User_SetServerAddr(AB_USER *u, const char *s) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  free(ue->serverAddr);
  if (s) ue->serverAddr=strdup(s);
  else ue->serverAddr=NULL;
}



int AO_User_GetServerPort(const AB_USER *u) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  return ue->serverPort;
}



void AO_User_SetServerPort(AB_USER *u, int i) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  ue->serverPort=i;
}



uint32_t AO_User_GetFlags(const AB_USER *u) {
  AO_USER *ue;

  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  return ue->flags;
}



void AO_User_SetFlags(AB_USER *u, uint32_t f) {
  AO_USER *ue;

  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  ue->flags=f;
}



void AO_User_AddFlags(AB_USER *u, uint32_t f) {
  AO_USER *ue;

  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  ue->flags|=f;
}



void AO_User_SubFlags(AB_USER *u, uint32_t f) {
  AO_USER *ue;

  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  ue->flags&=~f;
}



const char *AO_User_GetAppId(const AB_USER *u) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  return ue->appId;
}



void AO_User_SetAppId(AB_USER *u, const char *s) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  free(ue->appId);
  if (s) ue->appId=strdup(s);
  else ue->appId=NULL;
}



const char *AO_User_GetAppVer(const AB_USER *u) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  return ue->appVer;
}



void AO_User_SetAppVer(AB_USER *u, const char *s) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  free(ue->appVer);
  if (s) ue->appVer=strdup(s);
  else ue->appVer=NULL;
}



const char *AO_User_GetHeaderVer(const AB_USER *u) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  return ue->headerVer;
}



void AO_User_SetHeaderVer(AB_USER *u, const char *s) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  free(ue->headerVer);
  if (s) ue->headerVer=strdup(s);
  else ue->headerVer=NULL;
}



const char *AO_User_GetClientUid(const AB_USER *u) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  return ue->clientUid;
}



void AO_User_SetClientUid(AB_USER *u, const char *s) {
  AO_USER *ue;

  assert(u);
  ue=GWEN_INHERIT_GETDATA(AB_USER, AO_USER, u);
  assert(ue);

  free(ue->clientUid);
  if (s) ue->clientUid=strdup(s);
  else ue->clientUid=NULL;
}













