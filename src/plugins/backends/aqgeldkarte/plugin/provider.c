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


#include "provider_p.h"
#include "account_l.h"
#include "aqgeldkarte_l.h"

#include <aqbanking/account_be.h>
#include <aqbanking/job_be.h>
#include <aqbanking/jobgetbalance_be.h>
#include <aqbanking/jobgettransactions_be.h>
#include <aqbanking/value.h>

#include <gwenhywfar/debug.h>
#include <gwenhywfar/inherit.h>
#include <gwenhywfar/misc.h>
#include <gwenhywfar/bio_buffer.h>
#include <gwenhywfar/dbio.h>
#include <gwenhywfar/text.h>
#include <gwenhywfar/process.h>
#include <gwenhywfar/directory.h>
#include <gwenhywfar/waitcallback.h>
#include <gwenhywfar/gwentime.h>

#include <chipcard2-client/client/card.h>
#include <chipcard2-client/cards/geldkarte.h>
#include <chipcard2-client/cards/geldkarte_values.h>
#include <chipcard2-client/cards/geldkarte_blog.h>
#include <chipcard2-client/cards/geldkarte_llog.h>

#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

#ifdef HAVE_I18N
# ifdef HAVE_LOCALE_H
#  include <locale.h>
# endif
# ifdef HAVE_LIBINTL_H
#  include <libintl.h>
# endif

# define I18N(msg) dgettext(PACKAGE, msg)
#else
# define I18N(msg) msg
#endif

#define I18N_NOOP(msg) msg



GWEN_INHERIT(AB_PROVIDER, AG_PROVIDER)



AB_PROVIDER *AG_Provider_new(AB_BANKING *ab){
  AB_PROVIDER *pro;
  AG_PROVIDER *dp;

  pro=AB_Provider_new(ab, AG_PROVIDER_NAME);
  GWEN_NEW_OBJECT(AG_PROVIDER, dp);
  GWEN_INHERIT_SETDATA(AB_PROVIDER, AG_PROVIDER, pro, dp,
                       AG_Provider_FreeData);
  dp->cards=AG_Card_List_new();
  dp->bankingJobs=AB_Job_List2_new();

  AB_Provider_SetInitFn(pro, AG_Provider_Init);
  AB_Provider_SetFiniFn(pro, AG_Provider_Fini);
  AB_Provider_SetUpdateJobFn(pro, AG_Provider_UpdateJob);
  AB_Provider_SetAddJobFn(pro, AG_Provider_AddJob);
  AB_Provider_SetExecuteFn(pro, AG_Provider_Execute);
  AB_Provider_SetResetQueueFn(pro, AG_Provider_ResetQueue);
  AB_Provider_SetExtendAccountFn(pro, AG_Provider_ExtendAccount);

  /* register callback(s) */
  DBG_NOTICE(AQGELDKARTE_LOGDOMAIN, "Registering callbacks");

  return pro;
}



void AG_Provider_FreeData(void *bp, void *p) {
  AG_PROVIDER *dp;

  dp=(AG_PROVIDER*)p;
  assert(dp);

  AG_Card_List_free(dp->cards);
  AB_Job_List2_free(dp->bankingJobs);

  GWEN_FREE_OBJECT(dp);
}



int AG_Provider_Init(AB_PROVIDER *pro, GWEN_DB_NODE *dbData) {
  AG_PROVIDER *dp;
  const char *logLevelName;

  if (!GWEN_Logger_IsOpen(AQGELDKARTE_LOGDOMAIN)) {
    GWEN_Logger_Open(AQGELDKARTE_LOGDOMAIN,
		     "aqgeldkarte", 0,
		     GWEN_LoggerTypeConsole,
		     GWEN_LoggerFacilityUser);
  }

  logLevelName=getenv("AQGELDKARTE_LOGLEVEL");
  if (logLevelName) {
    GWEN_LOGGER_LEVEL ll;

    ll=GWEN_Logger_Name2Level(logLevelName);
    if (ll!=GWEN_LoggerLevelUnknown) {
      GWEN_Logger_SetLevel(AQGELDKARTE_LOGDOMAIN, ll);
      DBG_WARN(AQGELDKARTE_LOGDOMAIN,
               "Overriding loglevel for AqGeldKarte with \"%s\"",
               logLevelName);
    }
    else {
      DBG_ERROR(AQGELDKARTE_LOGDOMAIN, "Unknown loglevel \"%s\"",
                logLevelName);
    }
  }

  DBG_NOTICE(AQGELDKARTE_LOGDOMAIN, "Initializing AqGeldKarte backend");
  assert(pro);
  dp=GWEN_INHERIT_GETDATA(AB_PROVIDER, AG_PROVIDER, pro);
  assert(dp);


  dp->dbConfig=dbData;

  dp->chipcardClient=LC_Client_new(PACKAGE, VERSION, 0);
  if (LC_Client_ReadConfigFile(dp->chipcardClient, 0)) {
    DBG_ERROR(AQGELDKARTE_LOGDOMAIN,
              "Error loading chipcard2 client configuration file");
    return AB_ERROR_NOT_INIT;
  }

  return 0;
}



int AG_Provider_Fini(AB_PROVIDER *pro, GWEN_DB_NODE *dbData){
  AG_PROVIDER *dp;
  int errors=0;

  assert(pro);
  dp=GWEN_INHERIT_GETDATA(AB_PROVIDER, AG_PROVIDER, pro);
  assert(dp);

  DBG_NOTICE(AQGELDKARTE_LOGDOMAIN, "Deinitializing AqGELDKARTE backend");

  GWEN_DB_ClearGroup(dp->dbConfig, "accounts");

  dp->dbConfig=0;
  AB_Job_List2_Clear(dp->bankingJobs);
  AG_Card_List_Clear(dp->cards);

  LC_Client_free(dp->chipcardClient);
  dp->chipcardClient=0;

  if (errors)
    return AB_ERROR_GENERIC;

  return 0;
}



int AG_Provider_ExtendAccount(AB_PROVIDER *pro, AB_ACCOUNT *a,
                              AB_PROVIDER_EXTEND_MODE em){
  AG_Account_Extend(a, pro, em);
  return 0;
}



int AG_Provider_UpdateJob(AB_PROVIDER *pro, AB_JOB *j){
  AG_PROVIDER *dp;
  AB_ACCOUNT *da;

  assert(pro);
  dp=GWEN_INHERIT_GETDATA(AB_PROVIDER, AG_PROVIDER, pro);
  assert(dp);

  da=AB_Job_GetAccount(j);
  assert(da);

  switch(AB_Job_GetType(j)) {
  case AB_Job_TypeGetBalance:
    return 0;

  case AB_Job_TypeGetTransactions:
    return 0;

  default:
    DBG_INFO(AQGELDKARTE_LOGDOMAIN,
             "Job not supported (%d)",
             AB_Job_GetType(j));
    return AB_ERROR_NOT_SUPPORTED;
  } /* switch */
}



int AG_Provider_AddJob(AB_PROVIDER *pro, AB_JOB *bj){
  AG_PROVIDER *dp;
  AB_ACCOUNT *da;
  AG_CARD *card;

  assert(pro);
  dp=GWEN_INHERIT_GETDATA(AB_PROVIDER, AG_PROVIDER, pro);
  assert(dp);

  da=AB_Job_GetAccount(bj);
  assert(da);

  switch(AB_Job_GetType(bj)) {
  case AB_Job_TypeGetBalance:
  case AB_Job_TypeGetTransactions:
    break;
  default:
    DBG_INFO(AQGELDKARTE_LOGDOMAIN,
             "Job not supported (%d)",
             AB_Job_GetType(bj));
    return AB_ERROR_NOT_SUPPORTED;
  } /* switch */

  /* find card to which to add the job */
  card=AG_Card_List_First(dp->cards);
  while(card) {
    if (AG_Card_GetAccount(card)==da) {
      break;
    }
    card=AG_Card_List_Next(card);
  }

  if (!card) {
    /* no job matches, create new one */
    card=AG_Card_new(da);
    DBG_NOTICE(AQGELDKARTE_LOGDOMAIN,
	       "Adding job to account %s/%s, %s",
	       AB_Account_GetBankCode(da),
	       AB_Account_GetAccountNumber(da),
	       AG_Account_GetCardId(da));
    AG_Card_List_Add(card, dp->cards);
  }
  AG_Card_AddJob(card, bj);

  return 0;
}



int AG_Provider_ResetQueue(AB_PROVIDER *pro){
  AG_PROVIDER *dp;

  assert(pro);
  dp=GWEN_INHERIT_GETDATA(AB_PROVIDER, AG_PROVIDER, pro);
  assert(dp);

  AG_Card_List_Clear(dp->cards);
  AB_Job_List2_Clear(dp->bankingJobs);

  return 0;
}



int AG_Provider_GetBalance(AB_PROVIDER *pro,
                           LC_CARD *gc,
                           AB_JOB *bj) {
  AG_PROVIDER *dp;
  LC_GELDKARTE_VALUES *val;
  LC_CLIENT_RESULT res;

  assert(pro);
  dp=GWEN_INHERIT_GETDATA(AB_PROVIDER, AG_PROVIDER, pro);
  assert(dp);

  AB_Banking_ProgressLog(AB_Provider_GetBanking(pro), 0,
                         AB_Banking_LogLevelNotice,
                         I18N("Reading loaded amount"));
  val=LC_GeldKarte_Values_new();
  res=LC_GeldKarte_ReadValues(gc, val);
  if (res!=LC_Client_ResultOk) {
    DBG_ERROR(AQGELDKARTE_LOGDOMAIN, "Could not read values");
    LC_GeldKarte_Values_free(val);
    return AB_ERROR_GENERIC;
  }
  else {
    AB_ACCOUNT_STATUS *ast;
    GWEN_TIME *ti;
    AB_BALANCE *bal;
    AB_VALUE *v;

    ast=AB_AccountStatus_new();
    ti=GWEN_CurrentTime();
    assert(ti);
    AB_AccountStatus_SetTime(ast, ti);
    v=AB_Value_new((double)(LC_GeldKarte_Values_GetLoaded(val))/100.0, "EUR");
    assert(v);
    bal=AB_Balance_new(v, ti);
    assert(bal);
    AB_Value_free(v);
    GWEN_Time_free(ti);
    AB_AccountStatus_SetBookedBalance(ast, bal);
    AB_Balance_free(bal);

    AB_JobGetBalance_SetAccountStatus(bj, ast);
    AB_AccountStatus_free(ast);
    LC_GeldKarte_Values_free(val);
  }

  return 0;
}



int AG_Provider_GetTransactions(AB_PROVIDER *pro,
                                LC_CARD *gc,
                                AB_JOB *bj) {
  AG_PROVIDER *dp;
  LC_GELDKARTE_BLOG_LIST2 *blogs;
  LC_GELDKARTE_LLOG_LIST2 *llogs;
  LC_CLIENT_RESULT res;
  AB_TRANSACTION_LIST2 *tl;
  LC_GELDKARTE_BLOG_LIST2_ITERATOR *bit;
  LC_GELDKARTE_LLOG_LIST2_ITERATOR *lit;

  assert(pro);
  dp=GWEN_INHERIT_GETDATA(AB_PROVIDER, AG_PROVIDER, pro);
  assert(dp);

  AB_Banking_ProgressLog(AB_Provider_GetBanking(pro), 0,
                         AB_Banking_LogLevelNotice,
                         I18N("Reading business transactions"));

  tl=AB_Transaction_List2_new();
  blogs=LC_GeldKarte_BLog_List2_new();
  res=LC_GeldKarte_ReadBLogs(gc, blogs);
  if (res!=LC_Client_ResultOk) {
    if (res==LC_Client_ResultNoData) {
      AB_Banking_ProgressLog(AB_Provider_GetBanking(pro), 0,
			     AB_Banking_LogLevelNotice,
			     I18N("No business transactions"));
    }
    else {
      DBG_ERROR(AQGELDKARTE_LOGDOMAIN, "Could not read BLOGS");
      LC_GeldKarte_BLog_List2_freeAll(blogs);
      AB_Transaction_List2_freeAll(tl);
      AB_Job_SetStatus(bj, AB_Job_StatusError);
      AB_Job_SetResultText(bj, "Could not read BLOGs");
      AB_Banking_ProgressLog(AB_Provider_GetBanking(pro), 0,
			     AB_Banking_LogLevelError,
			     I18N("Error reading BLOG card transactions"));
      return AB_ERROR_GENERIC;
    }
  }

  bit=LC_GeldKarte_BLog_List2_First(blogs);
  if (bit) {
    LC_GELDKARTE_BLOG *blog;

    blog=LC_GeldKarte_BLog_List2Iterator_Data(bit);
    while(blog) {
      AB_TRANSACTION *t;
      AB_VALUE *v;
      double val;
      const char *s;
      char numbuf[64];

      t=AB_Transaction_new();

      val=(double)(LC_GeldKarte_BLog_GetValue(blog))/100.0;
      switch(LC_GeldKarte_BLog_GetStatus(blog) & 0x60) {
      case 0x60:
	s=I18N("STORNO");
	break;
      case 0x40:
	s=I18N("BUY");
	val=-val;
	break;
      case 0x20:
	s=I18N("CARD UNLOADED");
	val=-val;
	break;
      default:
	s=I18N("CARD LOADED");
	break;
      }
      v=AB_Value_new(val, "EUR");
      AB_Transaction_SetValue(t, v);
      AB_Value_free(v);

      /* create purpose */
      AB_Transaction_AddPurpose(t, s, 0);
      snprintf(numbuf, sizeof(numbuf), "BSEQ%04x LSEQ%04x",
	       LC_GeldKarte_BLog_GetBSeq(blog),
	       LC_GeldKarte_BLog_GetLSeq(blog));
      AB_Transaction_AddPurpose(t, numbuf, 0);
      snprintf(numbuf, sizeof(numbuf), "HSEQ%08x",
	       LC_GeldKarte_BLog_GetHSeq(blog));
      AB_Transaction_AddPurpose(t, numbuf, 0);
      snprintf(numbuf, sizeof(numbuf), "SSEQ%08x",
	       LC_GeldKarte_BLog_GetSSeq(blog));
      AB_Transaction_AddPurpose(t, numbuf, 0);

      s=LC_GeldKarte_BLog_GetMerchantId(blog);
      if (s)
	AB_Transaction_AddRemoteName(t, s, 0);
      AB_Transaction_SetDate(t, LC_GeldKarte_BLog_GetTime(blog));
      AB_Transaction_SetValutaDate(t, LC_GeldKarte_BLog_GetTime(blog));

      AB_Banking_ProgressLog(AB_Provider_GetBanking(pro), 0,
			     AB_Banking_LogLevelInfo,
			     I18N("Adding business transaction"));
      AB_Transaction_List2_PushBack(tl, t);

      blog=LC_GeldKarte_BLog_List2Iterator_Next(bit);
    } /* while */
    LC_GeldKarte_BLog_List2Iterator_free(bit);
  }
  LC_GeldKarte_BLog_List2_freeAll(blogs);

  /* read LLOGs */
  AB_Banking_ProgressLog(AB_Provider_GetBanking(pro), 0,
                         AB_Banking_LogLevelNotice,
                         I18N("Reading load/unload transactions"));
  llogs=LC_GeldKarte_LLog_List2_new();
  res=LC_GeldKarte_ReadLLogs(gc, llogs);
  if (res!=LC_Client_ResultOk) {
    if (res==LC_Client_ResultNoData) {
      AB_Banking_ProgressLog(AB_Provider_GetBanking(pro), 0,
			     AB_Banking_LogLevelNotice,
			     I18N("No load/unload transactions"));
    }
    else {
      DBG_ERROR(AQGELDKARTE_LOGDOMAIN, "Could not read LLOGS");
      LC_GeldKarte_LLog_List2_freeAll(llogs);
      AB_Transaction_List2_freeAll(tl);
      AB_Job_SetStatus(bj, AB_Job_StatusError);
      AB_Job_SetResultText(bj, "Could not read BLOGs");
      AB_Banking_ProgressLog(AB_Provider_GetBanking(pro), 0,
			     AB_Banking_LogLevelError,
			     I18N("Error reading BLOG card transactions"));
      return AB_ERROR_GENERIC;
    }
  }
  lit=LC_GeldKarte_LLog_List2_First(llogs);
  if (lit) {
    LC_GELDKARTE_LLOG *llog;

    llog=LC_GeldKarte_LLog_List2Iterator_Data(lit);
    while(llog) {
      AB_TRANSACTION *t;
      AB_VALUE *v;
      double val;
      const char *s;
      char numbuf[64];
      GWEN_BUFFER *buf;

      t=AB_Transaction_new();

      val=(double)(LC_GeldKarte_LLog_GetValue(llog))/100.0;

      switch(LC_GeldKarte_LLog_GetStatus(llog) & 0x60) {
      case 0x60:
	s=I18N("STORNO");
	break;
      case 0x40:
	s=I18N("BUY");
	val=-val;
	break;
      case 0x20:
	s=I18N("CARD UNLOADED");
	val=-val;
	break;
      default:
	s=I18N("CARD LOADED");
	break;
      }
      v=AB_Value_new(val, "EUR");
      AB_Transaction_SetValue(t, v);
      AB_Value_free(v);

      /* create purpose */
      AB_Transaction_AddPurpose(t, s, 0);
      snprintf(numbuf, sizeof(numbuf), "BSEQ%04x LSEQ%04x",
	       LC_GeldKarte_LLog_GetBSeq(llog),
	       LC_GeldKarte_LLog_GetLSeq(llog));
      AB_Transaction_AddPurpose(t, numbuf, 0);

      /* create name */
      buf=GWEN_Buffer_new(0, 32, 0, 1);
      s=LC_GeldKarte_LLog_GetCenterId(llog);
      if (s) {
	GWEN_Buffer_AppendString(buf, "CENTERID ");
	GWEN_Buffer_AppendString(buf, s);
	AB_Transaction_AddRemoteName(t, GWEN_Buffer_GetStart(buf), 0);
	GWEN_Buffer_Reset(buf);
      }
      s=LC_GeldKarte_LLog_GetTerminalId(llog);
      if (s) {
	GWEN_Buffer_AppendString(buf, "TERMINALID ");
	GWEN_Buffer_AppendString(buf, s);
	AB_Transaction_AddRemoteName(t, GWEN_Buffer_GetStart(buf), 0);
	GWEN_Buffer_Reset(buf);
      }
      s=LC_GeldKarte_LLog_GetTraceId(llog);
      if (s) {
	GWEN_Buffer_AppendString(buf, "TRACEID ");
	GWEN_Buffer_AppendString(buf, s);
	AB_Transaction_AddRemoteName(t, GWEN_Buffer_GetStart(buf), 0);
	GWEN_Buffer_Reset(buf);
      }
      GWEN_Buffer_free(buf);

      AB_Transaction_SetDate(t, LC_GeldKarte_LLog_GetTime(llog));
      AB_Transaction_SetValutaDate(t, LC_GeldKarte_LLog_GetTime(llog));

      AB_Banking_ProgressLog(AB_Provider_GetBanking(pro), 0,
			     AB_Banking_LogLevelInfo,
			     I18N("Adding load/unload transaction"));
      AB_Transaction_List2_PushBack(tl, t);

      llog=LC_GeldKarte_LLog_List2Iterator_Next(lit);
    } /* while */
    LC_GeldKarte_LLog_List2Iterator_free(lit);
  }
  LC_GeldKarte_LLog_List2_freeAll(llogs);

  AB_Job_SetResultText(bj, "Job exeuted successfully");
  AB_Banking_ProgressLog(AB_Provider_GetBanking(pro), 0,
                         AB_Banking_LogLevelNotice,
                         I18N("Job exeuted successfully"));
  AB_JobGetTransactions_SetTransactions(bj, tl);
  AB_Job_SetStatus(bj, AB_Job_StatusFinished);

  return 0;
}



int AG_Provider_ProcessCard(AB_PROVIDER *pro, AG_CARD *card){
  AG_PROVIDER *dp;
  AB_JOB_LIST2_ITERATOR *ait;

  assert(pro);
  dp=GWEN_INHERIT_GETDATA(AB_PROVIDER, AG_PROVIDER, pro);
  assert(dp);

  /* process jobs */
  ait=AB_Job_List2_First(AG_Card_GetBankingJobs(card));
  if (ait) {
    AB_JOB *bj;
    LC_CARD *gc;
    GWEN_TYPE_UINT32 bid;

    bid=AB_Banking_ShowBox(AB_Provider_GetBanking(pro),
                           0,
                           I18N("Accessing Card"),
                           I18N("Reading card, please wait..."));
    gc=AG_Provider_MountCard(pro, AG_Card_GetAccount(card));
    if (!gc) {
      DBG_ERROR(AQGELDKARTE_LOGDOMAIN, "Could not mount card");
      AB_Banking_HideBox(AB_Provider_GetBanking(pro), bid);
      return AB_ERROR_GENERIC;
    }
    bj=AB_Job_List2Iterator_Data(ait);
    assert(bj);
    while(bj) {
      int rv;

      switch(AB_Job_GetType(bj)) {
      case AB_Job_TypeGetBalance:
        rv=AG_Provider_GetBalance(pro, gc, bj);
        break;
      case AB_Job_TypeGetTransactions:
        rv=AG_Provider_GetTransactions(pro, gc, bj);
        break;
      default:
        /* not possible, but still ... */
        abort();
        break;
      }
      if (rv==AB_ERROR_USER_ABORT) {
        DBG_ERROR(AQGELDKARTE_LOGDOMAIN,
                  "User aborted, closing card");
        LC_Card_Close(gc);
        LC_Card_free(gc);
        AB_Job_List2Iterator_free(ait);
        AB_Banking_HideBox(AB_Provider_GetBanking(pro), bid);
        return rv;
      }
      bj=AB_Job_List2Iterator_Next(ait);
    } /* while */
    LC_Card_Close(gc);
    LC_Card_free(gc);
    AB_Job_List2Iterator_free(ait);
    AB_Banking_HideBox(AB_Provider_GetBanking(pro), bid);
  } /* if ait */

  return 0;
}



int AG_Provider_Execute(AB_PROVIDER *pro){
  AG_PROVIDER *dp;
  AG_CARD *card;
  int done=0;
  int succeeded=0;
  GWEN_TYPE_UINT32 pid;

  assert(pro);
  dp=GWEN_INHERIT_GETDATA(AB_PROVIDER, AG_PROVIDER, pro);
  assert(dp);

  pid=AB_Banking_ProgressStart(AB_Provider_GetBanking(pro),
                               I18N("Executing GeldKarte jobs"),
                               I18N("All GeldKarte jobs are now executed"),
                               AG_Card_List_GetCount(dp->cards));

  card=AG_Card_List_First(dp->cards);
  if (!card) {
    DBG_ERROR(AQGELDKARTE_LOGDOMAIN, "No cards");
  }
  done=0;
  while(card) {
    int rv;

    DBG_ERROR(AQGELDKARTE_LOGDOMAIN, "Handling card");
    AB_Banking_ProgressLog(AB_Provider_GetBanking(pro), 0,
                           AB_Banking_LogLevelNotice,
                           I18N("Handling job"));
    rv=AG_Provider_ProcessCard(pro, card);
    if (rv) {
      DBG_INFO(AQGELDKARTE_LOGDOMAIN, "Error processing card (%d)", rv);
      if (rv==AB_ERROR_USER_ABORT) {
        AB_Banking_ProgressLog(AB_Provider_GetBanking(pro), 0,
                               AB_Banking_LogLevelNotice,
                               I18N("User aborted"));
        AB_Banking_ProgressEnd(AB_Provider_GetBanking(pro), pid);
        return rv;
      }
    }
    else
      succeeded++;
    done++;
    if (AB_Banking_ProgressAdvance(AB_Provider_GetBanking(pro),
                                   0, done)) {
      DBG_ERROR(AQGELDKARTE_LOGDOMAIN, "User aborted via waitcallback");
      return AB_ERROR_USER_ABORT;
    }
    card=AG_Card_List_Next(card);
  }

  if (!succeeded && done) {
    DBG_ERROR(AQGELDKARTE_LOGDOMAIN, "Not a single job succeeded.");
  }
  AB_Banking_ProgressEnd(AB_Provider_GetBanking(pro), pid);
  return 0;
}



LC_CARD *AG_Provider_MountCard(AB_PROVIDER *pro, AB_ACCOUNT *acc){
  LC_CLIENT_RESULT res;
  GWEN_DB_NODE *dbCardData;
  LC_CARD *hcard;
  int first;
  const char *currCardNumber;
  AG_PROVIDER *dp;

  assert(pro);
  dp=GWEN_INHERIT_GETDATA(AB_PROVIDER, AG_PROVIDER, pro);
  assert(dp);

  res=LC_Client_StartWait(dp->chipcardClient, 0, 0);
  if (res!=LC_Client_ResultOk) {
    DBG_ERROR(AQGELDKARTE_LOGDOMAIN, "Could not send StartWait request");
    return 0;
  }

  first=1;
  for (;;) {
    int timeout;

    /* determine timeout value */
    if (first)
      timeout=3;
    else
      timeout=5;

    hcard=LC_Client_WaitForNextCard(dp->chipcardClient, timeout);
    if (!hcard) {
      int mres;

      mres=AB_Banking_MessageBox(AB_Provider_GetBanking(pro),
                                 AB_BANKING_MSG_FLAGS_TYPE_WARN |
                                 AB_BANKING_MSG_FLAGS_SEVERITY_DANGEROUS |
                                 AB_BANKING_MSG_FLAGS_CONFIRM_B1,
                                 I18N("Insert Card"),
                                 I18N("Please insert your chipcard into the "
                                      "reader and click OK"
                                      "<html>"
                                      "Please insert your chipcard into the "
                                      "reader and click <i>ok</i>"
                                      "</html>"),
                                 I18N("OK"), I18N("Abort"), 0);
      if (mres!=1) {
        DBG_ERROR(AQGELDKARTE_LOGDOMAIN, "Error in user interaction");
        LC_Client_StopWait(dp->chipcardClient);
        return 0;
      }
    }
    else {
      /* ok, we have a card, now check it */
      if (LC_GeldKarte_ExtendCard(hcard)) {
        DBG_ERROR(AQGELDKARTE_LOGDOMAIN,
                  "GeldKarte card not available, please check your setup");
        LC_Card_free(hcard);
        LC_Client_StopWait(dp->chipcardClient);
        return 0;
      }

      res=LC_Card_Open(hcard);
      if (res!=LC_Client_ResultOk) {
        LC_Card_free(hcard);
        DBG_NOTICE(AQGELDKARTE_LOGDOMAIN,
                   "Could not open card, maybe not a GeldKarte?");
      } /* if card not open */
      else {
        const char *mname;

        mname=AG_Account_GetCardId(acc);
        dbCardData=LC_GeldKarte_GetCardDataAsDb(hcard);
        assert(dbCardData);

        currCardNumber=GWEN_DB_GetCharValue(dbCardData,
                                            "cardNumber",
                                            0,
                                            0);
        if (!currCardNumber) {
          DBG_ERROR(AQGELDKARTE_LOGDOMAIN,
                    "INTERNAL: No card number in card data.");
	  abort();
	}

        DBG_NOTICE(AQGELDKARTE_LOGDOMAIN, "Card number: %s", currCardNumber);
        if (!mname || !*mname) {
          DBG_NOTICE(AQGELDKARTE_LOGDOMAIN, "No medium name");
          AG_Account_SetCardId(acc, currCardNumber);
          mname=AG_Account_GetCardId(acc);
          break;
        }

        if (strcasecmp(mname, currCardNumber)==0) {
          DBG_NOTICE(AQGELDKARTE_LOGDOMAIN, "Card number equals");
          break;
        }

        DBG_NOTICE(AQGELDKARTE_LOGDOMAIN,
                   "Card number does not equal ([%s] != [%s]",
                   mname, currCardNumber);

        LC_Card_Close(hcard);
	LC_Card_free(hcard);

        hcard=LC_Client_PeekNextCard(dp->chipcardClient);
        if (!hcard) {
          int mres;

          mres=AB_Banking_MessageBox(AB_Provider_GetBanking(pro),
                                     AB_BANKING_MSG_FLAGS_TYPE_WARN |
                                     AB_BANKING_MSG_FLAGS_SEVERITY_DANGEROUS |
                                     AB_BANKING_MSG_FLAGS_CONFIRM_B1,
                                     I18N("Insert Card"),
                                     I18N("Please insert the correct chipcard"
                                          " into the reader and click OK"
                                          "<html>"
                                          "Please insert the <b>correct</b>"
                                          " chipcard into the reader and "
                                          "click <i>ok</i>"
                                          "</html>"),
                                     I18N("OK"), I18N("Abort"), 0);
          if (mres!=1) {
            DBG_ERROR(AQGELDKARTE_LOGDOMAIN, "Error in user interaction");
            LC_Client_StopWait(dp->chipcardClient);
            return 0;
          }
        } /* if there is no other card waiting */
        else {
          /* otherwise there already is another card in another reader,
           * so no need to bother the user. This allows to insert all
           * cards in all readers and let me choose the card ;-) */
        } /* if there is another card waiting */
      } /* if card open */
    } /* if there is a card */

    first=0;
  } /* for */

  /* ok, now we have the card we wanted to have, now ask for the pin */
  LC_Client_StopWait(dp->chipcardClient);

  DBG_NOTICE(AQGELDKARTE_LOGDOMAIN, "Medium mounted");

  return hcard;
}














