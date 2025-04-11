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
 * Module: EduOM_PrevObject.c
 *
 * Description: 
 *  Return the previous object of the given current object.
 *
 * Exports:
 *  Four EduOM_PrevObject(ObjectID*, ObjectID*, ObjectID*, ObjectHdr*)
 */


#include <string.h>
#include "EduOM_common.h"
#include "BfM.h"
#include "EduOM_Internal.h"

/*@================================
 * EduOM_PrevObject()
 *================================*/
/*
 * Function: Four EduOM_PrevObject(ObjectID*, ObjectID*, ObjectID*, ObjectHdr*)
 *
 * Description: 
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  Return the previous object of the given current object. Find the object in
 *  the same page which has the current object and  if there  is no previous
 *  object in the same page, find it from the previous page.
 *  If the current object is NULL, return the last object of the file.
 *
 * Returns:
 *  error code
 *    eBADCATALOGOBJECT_OM
 *    eBADOBJECTID_OM
 *    some errors caused by function calls
 *
 * Side effect:
 *  1) parameter prevOID
 *     prevOID is filled with the previous object's identifier
 *  2) parameter objHdr
 *     objHdr is filled with the previous object's header
 */
Four EduOM_PrevObject(
    ObjectID *catObjForFile,	/* IN informations about a data file */
    ObjectID *curOID,		/* IN a ObjectID of the current object */
    ObjectID *prevOID,		/* OUT the previous object of a current object */
    ObjectHdr*objHdr)		/* OUT the object header of previous object */
{
    Four e;			/* error */
    Two  i;			/* index */
    Four offset;		/* starting offset of object within a page */
    PageID pid;			/* a page identifier */
    PageNo pageNo;		/* a temporary var for previous page's PageNo */
    SlottedPage *apage;		/* a pointer to the data page */
    Object *obj;		/* a pointer to the Object */
    SlottedPage *catPage;	/* buffer page containing the catalog object */
    sm_CatOverlayForData *catEntry; /* overlay structure for catalog object access */



    /*@ parameter checking */
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);
    
    if (prevOID == NULL) ERR(eBADOBJECTID_OM);

    if (e = BfM_GetTrain((TrainID*)catObjForFile, (char**)&catPage, PAGE_BUF) < 0) ERR(e);
    catEntry = (sm_CatOverlayForData*)(catPage->data + catPage->slot[-catObjForFile->slotNo].offset);
    if (!curOID){
        pid.volNo = catEntry->fid.volNo;
        pid.pageNo = catEntry->lastPage;
    }
    else {
        pid.volNo = curOID->volNo;
        pid.pageNo = curOID->pageNo;
    }

    while(1){
        if (e = BfM_GetTrain(&pid, (char**)&apage, PAGE_BUF < 0)) ERRB1(e, &pid, PAGE_BUF);
        Two startSlot = (curOID) ? (curOID->slotNo - 1) : (apage->header.nSlots - 1);
        for (i = startSlot; i >= 0; i--){
            if(apage->slot[-i].offset == EMPTYSLOT) continue;

            prevOID->pageNo = pid.pageNo;
            prevOID->volNo = pid.volNo;
            prevOID->slotNo = i;
            prevOID->unique = apage->slot[-i].unique;

            if (objHdr) {
                ObjectHdr *hdr = (ObjectHdr*)(apage->data + apage->slot[-i].offset);
                memcpy(objHdr, hdr, sizeof(ObjectHdr));
            }

            if (pid.pageNo == catEntry->firstPage) {
                BfM_FreeTrain((TrainID *) catObjForFile, PAGE_BUF);
                return (EOS);
            }

            PageNo prevPage = apage->header.prevPage;
            BfM_FreeTrain(&pid, PAGE_BUF);
            pid.pageNo = prevPage;
            curOID = NULL;
        }
    }
    
} /* EduOM_PrevObject() */
