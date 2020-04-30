/***************************************************************************
 begin       : Sun Oct 27 2019
 copyright   : (C) 2019 by Martin Preuss
 email       : martin@libchipcard.de

 ***************************************************************************
 * This file is part of the project "AqBanking".                           *
 * Please see toplevel file COPYING of that project for license details.   *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "sessionlayer/pintan/s_verify_pintan.h"
#include "sessionlayer/s_verify_rdh.h"
#include "sessionlayer/s_decode.h"
#include "parser/parser.h"

#include <gwenhywfar/misc.h>
#include <gwenhywfar/debug.h>
#include <gwenhywfar/text.h>



/* ------------------------------------------------------------------------------------------------
 * forward declarations
 * ------------------------------------------------------------------------------------------------
 */

static int _verifySegment(AQFINTS_SESSION *sess,
                          AQFINTS_SEGMENT *segSigHead,
                          AQFINTS_SEGMENT *segSigTail,
                          AQFINTS_SEGMENT *segFirstSigned,
                          AQFINTS_SEGMENT *segLastSigned);
static int _verifySegmentsRah(AQFINTS_SESSION *sess,
                              AQFINTS_SEGMENT *segSigHead,
                              AQFINTS_SEGMENT *segSigTail,
                              AQFINTS_SEGMENT *segFirstSigned,
                              AQFINTS_SEGMENT *segLastSigned,
                              int secProfileVersion,
                              const AQFINTS_KEYDESCR *keyDescr);
static int _verifySegmentsDdv(AQFINTS_SESSION *sess,
                              AQFINTS_SEGMENT *segSigHead,
                              AQFINTS_SEGMENT *segSigTail,
                              AQFINTS_SEGMENT *segFirstSigned,
                              AQFINTS_SEGMENT *segLastSigned,
                              int secProfileVersion,
                              const AQFINTS_KEYDESCR *keyDescr);

static AQFINTS_SEGMENT *_getFirstSegmentByCode(AQFINTS_SEGMENT_LIST *segmentList, const char *code);
static AQFINTS_SEGMENT *_getSigTailByControlReference(AQFINTS_SEGMENT *segment, const char *ctrlRef);
static AQFINTS_SEGMENT *_getFirstSignedSegment(AQFINTS_SEGMENT *segment);
static AQFINTS_SEGMENT *_getLastSignedSegment(AQFINTS_SEGMENT *segment);





/* ------------------------------------------------------------------------------------------------
 * implementations
 * ------------------------------------------------------------------------------------------------
 */



int AQFINTS_Session_VerifySegmentList(AQFINTS_SESSION *sess, AQFINTS_SEGMENT_LIST *segmentList)
{
  AQFINTS_SEGMENT *segSigHead;
  AQFINTS_SEGMENT *segFirstSigned;
  AQFINTS_SEGMENT *segLastSigned;
  AQFINTS_SEGMENT *segment;

  segSigHead=_getFirstSegmentByCode(segmentList, "HNSHK");
  if (segSigHead==NULL) {
    DBG_INFO(AQFINTS_LOGDOMAIN, "No signatures in segment list");
    return 0;
  }

  segFirstSigned=_getFirstSignedSegment(segSigHead);
  if (segFirstSigned==NULL) {
    DBG_INFO(AQFINTS_LOGDOMAIN, "No signed segments in segment list");
    return GWEN_ERROR_BAD_DATA;
  }
  segLastSigned=_getLastSignedSegment(segFirstSigned);
  if (segLastSigned==NULL) {
    DBG_INFO(AQFINTS_LOGDOMAIN, "No signature tail in segment list");
    return GWEN_ERROR_BAD_DATA;
  }

  while(segSigHead && segSigHead!=segFirstSigned) {
    GWEN_DB_NODE *dbSigHead;
    const char *sCtrlRef;
    AQFINTS_SEGMENT *segSigTail;
    int rv;

    dbSigHead=AQFINTS_Segment_GetDbData(segSigHead);
    assert(dbSigHead);

    sCtrlRef=GWEN_DB_GetCharValue(dbSigHead, "ctrlref", 0, NULL);
    if (!(sCtrlRef && *sCtrlRef)) {
      DBG_ERROR(AQFINTS_LOGDOMAIN, "No control reference in signature head");
      return GWEN_ERROR_BAD_DATA;
    }

    segSigTail=_getSigTailByControlReference(segLastSigned, sCtrlRef);
    if (segSigTail==NULL) {
      DBG_ERROR(AQFINTS_LOGDOMAIN, "No matching signature tail with reference \"%s\" found", sCtrlRef);
      return GWEN_ERROR_BAD_DATA;
    }

    rv=_verifySegment(sess, segSigHead, segSigTail, segFirstSigned, segLastSigned);
    if (rv<0) {
      DBG_INFO(AQFINTS_LOGDOMAIN, "here (%d)", rv);
      return rv;
    }

    segSigHead=AQFINTS_Segment_List_Next(segSigHead);
  }

  /* mark segments as signed */
  segment=segFirstSigned;
  while(segment) {
    AQFINTS_Segment_AddRuntimeFlags(segment, AQFINTS_SEGMENT_RTFLAGS_SIGNED);
    if (segment==segLastSigned)
      break;
    segment=AQFINTS_Segment_List_Next(segment);
  }

  /* done */
  return 0;
}



int _verifySegment(AQFINTS_SESSION *sess,
                   AQFINTS_SEGMENT *segSigHead,
                   AQFINTS_SEGMENT *segSigTail,
                   AQFINTS_SEGMENT *segFirstSigned,
                   AQFINTS_SEGMENT *segLastSigned)
{
  GWEN_DB_NODE *dbSigHead;
  GWEN_DB_NODE *dbSigTail;
  AQFINTS_KEYDESCR *keyDescr;
  const char *s;
  int v;

  dbSigHead=AQFINTS_Segment_GetDbData(segSigHead);
  assert(dbSigHead);

  dbSigTail=AQFINTS_Segment_GetDbData(segSigTail);
  assert(dbSigTail);

  /* setup key name */
  keyDescr=AQFINTS_Session_ReadKeyDescrFromDbHead(dbSigHead);
  s=GWEN_DB_GetCharValue(dbSigTail, "pin", 0, NULL);
  AQFINTS_KeyDescr_SetPin(keyDescr, s);
  s=GWEN_DB_GetCharValue(dbSigTail, "tan", 0, NULL);
  AQFINTS_KeyDescr_SetTan(keyDescr, s);

  /* call appropriate function according to security profile */
  v=GWEN_DB_GetIntValue(dbSigHead, "secProfile/version", 0, 0);
  s=GWEN_DB_GetCharValue(dbSigHead, "secProfile/code", 0, NULL);
  if (s && *s) {
    int rv;

    DBG_INFO(AQFINTS_LOGDOMAIN, "Selected security profile is \"%s\" (version %d)", s, v);
    if (strcasecmp(s, "PINTAN")==0)
      rv=AQFINTS_Session_VerifySegmentsPinTan(sess, segSigHead, segSigTail, segFirstSigned, segLastSigned, v, keyDescr);
    else if (strcasecmp(s, "RDH")==0)
      rv=AQFINTS_Session_VerifySegmentsRdh(sess, segSigHead, segSigTail, segFirstSigned, segLastSigned, v, keyDescr);
    else if (strcasecmp(s, "RAH")==0)
      rv=_verifySegmentsRah(sess, segSigHead, segSigTail, segFirstSigned, segLastSigned, v, keyDescr);
    else if (strcasecmp(s, "DDV")==0)
      rv=_verifySegmentsDdv(sess, segSigHead, segSigTail, segFirstSigned, segLastSigned, v, keyDescr);
    else {
      DBG_ERROR(AQFINTS_LOGDOMAIN, "Invalid security profile \"%s\"", s);
      AQFINTS_KeyDescr_free(keyDescr);
      return GWEN_ERROR_BAD_DATA;
    }
    AQFINTS_KeyDescr_free(keyDescr);
    if (rv<0) {
      DBG_ERROR(0, "here (%d)", rv);
      return rv;
    }
    return 0;
  }
  else {
    DBG_ERROR(AQFINTS_LOGDOMAIN, "Missing security profile code");
    return GWEN_ERROR_BAD_DATA;
  }
}



int _verifySegmentsRah(AQFINTS_SESSION *sess,
                       AQFINTS_SEGMENT *segSigHead,
                       AQFINTS_SEGMENT *segSigTail,
                       AQFINTS_SEGMENT *segFirstSigned,
                       AQFINTS_SEGMENT *segLastSigned,
                       int secProfileVersion,
                       const AQFINTS_KEYDESCR *keyDescr)
{
  return GWEN_ERROR_NOT_IMPLEMENTED;
}



int _verifySegmentsDdv(AQFINTS_SESSION *sess,
                       AQFINTS_SEGMENT *segSigHead,
                       AQFINTS_SEGMENT *segSigTail,
                       AQFINTS_SEGMENT *segFirstSigned,
                       AQFINTS_SEGMENT *segLastSigned,
                       int secProfileVersion,
                       const AQFINTS_KEYDESCR *keyDescr)
{
  return GWEN_ERROR_NOT_IMPLEMENTED;
}





AQFINTS_SEGMENT *_getFirstSegmentByCode(AQFINTS_SEGMENT_LIST *segmentList, const char *code)
{
  AQFINTS_SEGMENT *segment;

  segment=AQFINTS_Segment_List_First(segmentList);
  while(segment) {
    const char *s;

    s=AQFINTS_Segment_GetCode(segment);
    if (s && *s && strcasecmp(s, code)==0)
      return segment;
    segment=AQFINTS_Segment_List_Next(segment);
  }

  return NULL;
}



AQFINTS_SEGMENT *_getSigTailByControlReference(AQFINTS_SEGMENT *segment, const char *ctrlRef)
{
  while(segment) {
    const char *sCode;

    sCode=AQFINTS_Segment_GetCode(segment);
    if (sCode && *sCode && strcasecmp(sCode, "HNSHA")==0) {
      GWEN_DB_NODE *dbSigTail;
      const char *s;

      dbSigTail=AQFINTS_Segment_GetDbData(segment);
      assert(dbSigTail);

      s=GWEN_DB_GetCharValue(dbSigTail, "ctrlref", 0, NULL);
      if (s && *s && strcasecmp(s, ctrlRef)==0)
        return segment;
    }
    segment=AQFINTS_Segment_List_Next(segment);
  }

  return NULL;
}



AQFINTS_SEGMENT *_getFirstSignedSegment(AQFINTS_SEGMENT *segment)
{
  while(segment) {
    const char *sCode;

    sCode=AQFINTS_Segment_GetCode(segment);
    if (!(sCode && *sCode && strcasecmp(sCode, "HNSHK")==0))
      return segment;
    segment=AQFINTS_Segment_List_Next(segment);
  }

  return NULL;
}



AQFINTS_SEGMENT *_getLastSignedSegment(AQFINTS_SEGMENT *segment)
{
  while(segment) {
    const char *sCode;

    sCode=AQFINTS_Segment_GetCode(segment);
    if (!(sCode && *sCode && strcasecmp(sCode, "HNSHA")==0))
      return segment;
    segment=AQFINTS_Segment_List_Next(segment);
  }

  return NULL;
}








