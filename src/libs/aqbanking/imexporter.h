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


#ifndef AQBANKING_IMEXPORTER_H
#define AQBANKING_IMEXPORTER_H

#include <gwenhywfar/inherit.h>
#include <gwenhywfar/bufferedio.h>
#include <gwenhywfar/db.h>
#include <gwenhywfar/types.h>
#include <aqbanking/error.h>
#include <aqbanking/accstatus.h>


/** @defgroup AB_IMEXPORTER Generic Im- and Exporter
 * @short Generic Financial Data Importer/Exporter
 * <p>
 * This group contains a generic importer/exporter.
 * </p>
 * <h2>Importing</h2>
 * <p>
 * When importing this group reads transactions and accounts from a
 * given stream (in most cases a file) and stores them in a given
 * importer context.
 * </p>
 * <p>
 * The application can later browse through all transactions stored within the
 * given context and import them into its own database as needed.
 * </p>
 */
/*@{*/

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AB_IMEXPORTER AB_IMEXPORTER;
GWEN_INHERIT_FUNCTION_LIB_DEFS(AB_IMEXPORTER, AQBANKING_API)

typedef struct AB_IMEXPORTER_CONTEXT AB_IMEXPORTER_CONTEXT;
typedef struct AB_IMEXPORTER_ACCOUNTINFO AB_IMEXPORTER_ACCOUNTINFO;

#ifdef __cplusplus
}
#endif


#include <aqbanking/banking.h>
#include <aqbanking/account.h>
#include <aqbanking/transaction.h>


#ifdef __cplusplus
extern "C" {
#endif


/** @name Virtual Functions for Backends
 *
 */
/*@{*/

/**
 * Reads the given stream and imports all data from it. This imported
 * data is stored within the given context.
 * @param ie pointer to the importer/exporter
 * @param ctx import context
 * @param bio stream to read from (usually a file, see
 *   @ref GWEN_BufferedIO_File_new)
 * @param dbProfile configuration data for the importer. You can get this
 *   using @ref AB_Banking_GetImExporterProfiles.
 */
AQBANKING_API 
int AB_ImExporter_Import(AB_IMEXPORTER *ie,
                         AB_IMEXPORTER_CONTEXT *ctx,
                         GWEN_BUFFEREDIO *bio,
                         GWEN_DB_NODE *dbProfile);
/*@}*/



AQBANKING_API 
AB_BANKING *AB_ImExporter_GetBanking(const AB_IMEXPORTER *ie);
AQBANKING_API 
const char *AB_ImExporter_GetName(const AB_IMEXPORTER *ie);




/** @name Im-/export Context
 *
 * A context contains the list of accounts for which data has been imported
 * or which are to be exported.
 * These accounts each contain a list of imported/to be exported
 * transactions.
 */
/*@{*/
AQBANKING_API 
AB_IMEXPORTER_CONTEXT *AB_ImExporterContext_new();
AQBANKING_API 
void AB_ImExporterContext_free(AB_IMEXPORTER_CONTEXT *iec);

/**
 * Takes over ownership of the given account info.
 */
AQBANKING_API 
void AB_ImExporterContext_AddAccountInfo(AB_IMEXPORTER_CONTEXT *iec,
                                         AB_IMEXPORTER_ACCOUNTINFO *iea);

/**
 * Returns the first imported account (if any).
 * The caller becomes the new owner of the account info returned (if any),
 * so he/she is responsible for calling @ref AB_Account_free() when finished.
 */
AQBANKING_API 
AB_IMEXPORTER_ACCOUNTINFO*
AB_ImExporterConect_GetFirstAccountInfo(AB_IMEXPORTER_CONTEXT *iec);

/**
 * Returns the next account data has been imported for.
 * The caller becomes the new owner of the account info returned (if any),
 * so he/she is responsible for calling @ref AB_Account_free() when finished.
 */
AQBANKING_API 
AB_IMEXPORTER_ACCOUNTINFO*
AB_ImExporterContext_GetNextAccountInfo(AB_IMEXPORTER_CONTEXT *iec);

/**
 * Looks for account info for the given account. If there is none it will
 * be created and added to the context.
 * The context remains the owner of the returned object.
 */
AQBANKING_API
AB_IMEXPORTER_ACCOUNTINFO*
AB_ImExporterContext_GetAccountInfo(AB_IMEXPORTER_CONTEXT *iec,
                                    const char *bankCode,
                                    const char *accountNumber);

/**
 * This is just a conveniance function. It takes the bank code and
 * account number from the account, and then calls
 * @ref AB_ImExporterContext_GetAccountInfo and
 * @ref AB_ImExporterAccountInfo_AddTransaction.
 * If you want to add many transactions which are sorted by account
 * it is much faster to avoid this function and to select the appropriate
 * account info object once before importing all transactions for this
 * particular account. This would save you the additional lookup before
 * every transaction.
 */
AQBANKING_API
void AB_ImExporterContext_AddTransaction(AB_IMEXPORTER_CONTEXT *iec,
                                         AB_TRANSACTION *t);


/*@}*/



/** @name Im-/export Account Info
 *
 * Such a structure contains the list of imported/to be exported transactions
 * for a given account.
 */
/*@{*/
AQBANKING_API 
AB_IMEXPORTER_ACCOUNTINFO *AB_ImExporterAccountInfo_new();
AQBANKING_API 
void AB_ImExporterAccountInfo_free(AB_IMEXPORTER_ACCOUNTINFO *iea);
/**
 * Takes over ownership of the given transaction.
 */
AQBANKING_API 
void AB_ImExporterAccountInfo_AddTransaction(AB_IMEXPORTER_ACCOUNTINFO *iea,
                                             AB_TRANSACTION *t);
/**
 * Returns the first transaction stored within the context.
 * The caller becomes the new owner of the transaction returned (if any)
 * which makes him/her responsible for freeing it using
 * @ref AB_Transaction_free.
 */
AQBANKING_API 
AB_TRANSACTION*
AB_ImExporterAccountInfo_GetFirstTransaction(AB_IMEXPORTER_ACCOUNTINFO *iea);
/**
 * Returns the next transaction stored within the context.
 * The caller becomes the new owner of the transaction returned (if any)
 * which makes him/her responsible for freeing it using
 * @ref AB_Transaction_free.
 */
AQBANKING_API 
AB_TRANSACTION*
AB_ImExporterAccountInfo_GetNextTransaction(AB_IMEXPORTER_ACCOUNTINFO *iea);


/**
 * Takes over ownership of the given account status.
 */
AQBANKING_API 
void AB_ImExporterAccountInfo_AddAccountStatus(AB_IMEXPORTER_ACCOUNTINFO *iea,
                                               AB_ACCOUNT_STATUS *st);
/**
 * Returns the first account status stored within the context.
 * The caller becomes the new owner of the account status returned (if any)
 * which makes him/her responsible for freeing it using
 * @ref AB_AccountStatus_free.
 */
AQBANKING_API 
AB_ACCOUNT_STATUS*
AB_ImExporterAccountInfo_GetFirstAccountStatus(AB_IMEXPORTER_ACCOUNTINFO *iea);
/**
 * Returns the next account status stored within the context.
 * The caller becomes the new owner of the account status returned (if any)
 * which makes him/her responsible for freeing it using
 * @ref AB_AccountStatus_free.
 */
AQBANKING_API 
AB_ACCOUNT_STATUS*
AB_ImExporterAccountInfo_GetNextAccountStatus(AB_IMEXPORTER_ACCOUNTINFO *iea);


/**
 * This is used when exporting an account. Not used for imports.
 */
AQBANKING_API 
AB_ACCOUNT*
AB_ImExporterAccountInfo_GetAccount(const AB_IMEXPORTER_ACCOUNTINFO *iea);
void AB_ImExporterAccountInfo_SetAccount(AB_IMEXPORTER_ACCOUNTINFO *iea,
                                         AB_ACCOUNT *a);

/**
 * Bank code of the institute the account is at .
 * Used when importing data, not used when exporting.
 */
AQBANKING_API 
const char*
AB_ImExporterAccountInfo_GetBankCode(const AB_IMEXPORTER_ACCOUNTINFO *iea);
void AB_ImExporterAccountInfo_SetBankCode(AB_IMEXPORTER_ACCOUNTINFO *iea,
                                          const char *s);

/**
 * Bank name of the institute the account is at .
 * Used when importing data, not used when exporting.
 */
AQBANKING_API 
const char*
AB_ImExporterAccountInfo_GetBankName(const AB_IMEXPORTER_ACCOUNTINFO *iea);
void AB_ImExporterAccountInfo_SetBankName(AB_IMEXPORTER_ACCOUNTINFO *iea,
                                          const char *s);

/**
 * Account number.
 * Used when importing data, not used when exporting.
 */
AQBANKING_API 
const char*
AB_ImExporterAccountInfo_GetAccountNumber(const AB_IMEXPORTER_ACCOUNTINFO *iea);
void AB_ImExporterAccountInfo_SetAccountNumber(AB_IMEXPORTER_ACCOUNTINFO *iea,
                                               const char *s);

/**
 * Account name.
 * Used when importing data, not used when exporting.
 */
AQBANKING_API 
const char*
AB_ImExporterAccountInfo_GetAccountName(const AB_IMEXPORTER_ACCOUNTINFO *iea);
void AB_ImExporterAccountInfo_SetAccountName(AB_IMEXPORTER_ACCOUNTINFO *iea,
                                             const char *s);

/**
 * Name of the account' owner.
 * Used when importing data, not used when exporting.
 */
AQBANKING_API 
const char*
AB_ImExporterAccountInfo_GetOwner(const AB_IMEXPORTER_ACCOUNTINFO *iea);
void AB_ImExporterAccountInfo_SetOwner(AB_IMEXPORTER_ACCOUNTINFO *iea,
                                       const char *s);


/*@}*/



#ifdef __cplusplus
}
#endif


/*@}*/ /* defgroup */


#endif /* AQBANKING_IMEXPORTER_H */


