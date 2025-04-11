/******************************************************************************/
/*                                                                            */
/*    ODYSSEUS/EduCOSMOS Educational-Purpose Object Storage System            */
/*                                                                            */
/*    Developed by Professor Kyu-Young Whang et al.                           */
/*                                                                            */
/*    Database and Multimedia Laboratory                                      */
/*                                                                            */
/*    Computer Science Department and                                         */
/*    Advanced Information Technology Research Center (AITrc)                 */
/*    Korea Advanced Institute of Science and Technology (KAIST)              */
/*                                                                            */
/*    e-mail: kywhang@cs.kaist.ac.kr                                          */
/*    phone: +82-42-350-7722                                                  */
/*    fax: +82-42-350-8380                                                    */
/*                                                                            */
/*    Copyright (c) 1995-2013 by Kyu-Young Whang                              */
/*                                                                            */
/*    All rights reserved. No part of this software may be reproduced,        */
/*    stored in a retrieval system, or transmitted, in any form or by any     */
/*    means, electronic, mechanical, photocopying, recording, or otherwise,   */
/*    without prior written permission of the copyright owner.                */
/*                                                                            */
/******************************************************************************/
/*
 * Module : EduOM_CreateObject.c
 * 
 * Description :
 *  EduOM_CreateObject() creates a new object near the specified object.
 *
 * Exports:
 *  Four EduOM_CreateObject(ObjectID*, ObjectID*, ObjectHdr*, Four, char*, ObjectID*)
 */

#include <string.h>
#include "EduOM_common.h"
#include "RDsM.h"		/* for the raw disk manager call */
#include "BfM.h"		/* for the buffer manager call */
#include "EduOM_Internal.h"
#include "EduOM.h"

/*@================================
 * EduOM_CreateObject()
 *================================*/
/*
 * Function: Four EduOM_CreateObject(ObjectID*, ObjectID*, ObjectHdr*, Four, char*, ObjectID*)
 * 
 * Description :
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 * (1) What to do?
 * EduOM_CreateObject() creates a new object near the specified object.
 * If there is no room in the page holding the specified object,
 * it trys to insert into the page in the available space list. If fail, then
 * the new object will be put into the newly allocated page.
 *
 * (2) How to do?
 *	a. Read in the near slotted page
 *	b. See the object header
 *	c. IF large object THEN
 *	       call the large object manager's lom_ReadObject()
 *	   ELSE 
 *		   IF moved object THEN 
 *				call this function recursively
 *		   ELSE 
 *				copy the data into the buffer
 *		   ENDIF
 *	   ENDIF
 *	d. Free the buffer page
 *	e. Return
 *
 * Returns:
 *  error code
 *    eBADCATALOGOBJECT_OM
 *    eBADLENGTH_OM
 *    eBADUSERBUF_OM
 *    some error codes from the lower level
 *
 * Side Effects :
 *  0) A new object is created.
 *  1) parameter oid
 *     'oid' is set to the ObjectID of the newly created object.
 */
Four EduOM_CreateObject(
    ObjectID  *catObjForFile,	/* IN file in which object is to be placed */
    ObjectID  *nearObj,		/* IN create the new object near this object */
    ObjectHdr *objHdr,		/* IN from which tag is to be set */
    Four      length,		/* IN amount of data */
    char      *data,		/* IN the initial data for the object */
    ObjectID  *oid)		/* OUT the object's ObjectID */
{
    Four        e;		/* error number */
    ObjectHdr   objectHdr;	/* ObjectHdr with tag set from parameter */


    /*@ parameter checking */
    
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);
    
    if (length < 0) ERR(eBADLENGTH_OM);

    if (length > 0 && data == NULL) return(eBADUSERBUF_OM);

	/* Error check whether using not supported functionality by EduOM */
	if(ALIGNED_LENGTH(length) > LRGOBJ_THRESHOLD) ERR(eNOTSUPPORTED_EDUOM);

    objectHdr.properties = 0x0;
    length = 0;
    if(objHdr == NULL)
        objectHdr.tag = 0;
    else
        objHdr->tag = objHdr->tag;
    return eduom_CreateObject(catObjForFile,
                       nearObj,
                       objHdr,
                       length,
                       data,
                       oid);

    //return(eNOERROR);
}

/*@================================
 * eduom_CreateObject()
 *================================*/
/*
 * Function: Four eduom_CreateObject(ObjectID*, ObjectID*, ObjectHdr*, Four, char*, ObjectID*)
 *
 * Description :
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  eduom_CreateObject() creates a new object near the specified object; the near
 *  page is the page holding the near object.
 *  If there is no room in the near page and the near object 'nearObj' is not
 *  NULL, a new page is allocated for object creation (In this case, the newly
 *  allocated page is inserted after the near page in the list of pages
 *  consiting in the file).
 *  If there is no room in the near page and the near object 'nearObj' is NULL,
 *  it trys to create a new object in the page in the available space list. If
 *  fail, then the new object will be put into the newly allocated page(In this
 *  case, the newly allocated page is appended at the tail of the list of pages
 *  cosisting in the file).
 *
 * Returns:
 *  error Code
 *    eBADCATALOGOBJECT_OM
 *    eBADOBJECTID_OM
 *    some errors caused by fuction calls
 */
Four eduom_CreateObject(
                        ObjectID	*catObjForFile,	/* IN file in which object is to be placed */
                        ObjectID 	*nearObj,	/* IN create the new object near this object */
                        ObjectHdr	*objHdr,	/* IN from which tag & properties are set */
                        Four	length,		/* IN amount of data */
                        char	*data,		/* IN the initial data for the object */
                        ObjectID	*oid)		/* OUT the object's ObjectID */
{
    Four        e;		/* error number */
    Four	neededSpace;	/* space needed to put new object [+ header] */
    SlottedPage *apage;		/* pointer to the slotted page buffer */
    Four        alignedLen;	/* aligned length of initial data */
    Boolean     needToAllocPage;/* Is there a need to alloc a new page? */
    PageID      pid;            /* PageID in which new object to be inserted */
    PageID      nearPid;
    Four        firstExt;	/* first Extent No of the file */
    Object      *obj;		/* point to the newly created object */
    Two         i;		/* index variable */
    sm_CatOverlayForData *catEntry; /* pointer to data file catalog information */
    SlottedPage *catPage;	/* pointer to buffer containing the catalog */
    FileID      fid;		/* ID of file where the new object is placed */
    Two         eff;		/* extent fill factor of file */
    Boolean     isTmp;
    PhysicalFileID pFid;
    ShortPageID available[] =
            {catEntry->availSpaceList10,
             catEntry->availSpaceList20,
             catEntry->availSpaceList30,
             catEntry->availSpaceList40,
             catEntry->availSpaceList50};
    
    /*@ parameter checking */
    
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);
    
    if (objHdr == NULL) ERR(eBADOBJECTID_OM);
    
    /* Error check whether using not supported functionality by EduOM */
    if(ALIGNED_LENGTH(length) > LRGOBJ_THRESHOLD) ERR(eNOTSUPPORTED_EDUOM);

    alignedLen = ALIGNED_LENGTH(length);
    neededSpace = sizeof(ObjectHdr) + alignedLen + sizeof(SlottedPageSlot);
    if((e = BfM_GetTrain((TrainID*)catObjForFile, (char**)&catPage, PAGE_BUF)) < 0)
        ERRB1(e, (TrainID*)catObjForFile, PAGE_BUF);
    catEntry = (sm_CatOverlayForData*)(catPage->data + catPage->slot[-catObjForFile->slotNo].offset);
    if (nearObj) {
        if ((e = BfM_GetTrain((TrainID *) nearObj, (char **) &apage, PAGE_BUF)) < 0)
            ERRB1(e, (TrainID *) catObjForFile, PAGE_BUF);
        nearPid.pageNo = nearObj->pageNo;
        nearPid.volNo = nearObj->volNo;
        needToAllocPage = neededSpace > (apage->header.free + apage->header.unused);
        if (!needToAllocPage) {
            om_RemoveFromAvailSpaceList(catObjForFile, &nearPid, apage);
            pid = nearPid;
            if (neededSpace < apage->header.free)
                EduOM_CompactPage(apage, nearObj->slotNo);
        } else {
            MAKE_PHYSICALFILEID(pFid, catEntry->fid.volNo, catEntry->firstPage);
            if ((e = RDsM_PageIdToExtNo((PageID*)&pFid, &firstExt)) < 0) ERR(e);
            if ((e = RDsM_AllocTrains(nearObj->volNo, firstExt, &nearPid, catEntry->eff, 1, PAGESIZE2, &pid)) < 0)
                ERRB1(e, (TrainID*)nearObj, PAGE_BUF);
            om_FileMapAddPage(catObjForFile, &nearPid, &pid);
            BfM_GetNewTrain(&pid, (char**)&apage, PAGE_BUF);
            apage->header.pid = pid;
            apage->header.free = sizeof(SlottedPageHdr);
        }
    } else {
        isTmp = FALSE;
        for (i = 0; i < 5; i++) {
            if (available[i] &&
                (neededSpace <= (i + 1)*PAGESIZE)) {
                pid.pageNo = available[i];
                pid.volNo = catEntry->fid.volNo;
                isTmp = TRUE;
                break;
            }
        }
        if(!isTmp){
            MAKE_PHYSICALFILEID(pFid, catEntry->fid.volNo, catEntry->firstPage);
            if ((e = RDsM_PageIdToExtNo((PageID*)&pFid, &firstExt)) < 0) ERR(e);
            if ((e = RDsM_AllocTrains(catEntry->fid.volNo, firstExt, NULL,
                                 catEntry->eff, 1, PAGESIZE2, &pid)) < 0)
                ERR(e);
            om_FileMapAddPage(catObjForFile, NULL, &pid);
        }else {
            if (neededSpace < apage->header.free)
                EduOM_CompactPage(apage, NIL);
        }
        if ((e = BfM_GetTrain(&pid, (char**)&apage, PAGE_BUF)) < 0) ERR(e);
    }



    /* Update page metadata */
    apage->header.nSlots++;
    BfM_SetDirty(&pid, PAGE_BUF);

    /* Copy object data */
    obj = apage->data + apage->header.free;
    apage->header.free += sizeof(ObjectHdr) + alignedLen;
    obj->header.length = length;
    obj->header.tag = objHdr->tag;
    memcpy(obj->data, data, length);

    //insert the object
    apage->slot[-apage->header.nSlots].offset = apage->header.free;
    om_GetUnique(&pid, &apage->slot[-apage->header.nSlots].unique);

    /* Update oid */
    oid->pageNo = pid.pageNo;
    oid->volNo = pid.volNo;
    oid->slotNo = apage->header.nSlots;
    oid->unique = apage->slot[apage->header.nSlots].unique;

    om_PutInAvailSpaceList(catObjForFile, &pid, apage);

    if ((e = BfM_FreeTrain(&pid, PAGE_BUF)) < 0) ERR(e);
    if ((e = BfM_FreeTrain((TrainID*)catObjForFile, PAGE_BUF)) < 0) ERR(e);
    return(eNOERROR);
    
} /* eduom_CreateObject() */
