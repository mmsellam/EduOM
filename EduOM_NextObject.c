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
 * Module: EduOM_NextObject.c
 *
 * Description:
 *  Return the next Object of the given Current Object. 
 *
 * Export:
 *  Four EduOM_NextObject(ObjectID*, ObjectID*, ObjectID*, ObjectHdr*)
 */


#include <string.h>
#include "EduOM_common.h"
#include "BfM.h"
#include "EduOM_Internal.h"

/*@================================
 * EduOM_NextObject()
 *================================*/
/*
 * Function: Four EduOM_NextObject(ObjectID*, ObjectID*, ObjectID*, ObjectHdr*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  Return the next Object of the given Current Object.  Find the Object in the
 *  same page which has the current Object and  if there  is no next Object in
 *  the same page, find it from the next page. If the Current Object is NULL,
 *  return the first Object of the file.
 *
 * Returns:
 *  error code
 *    eBADCATALOGOBJECT_OM
 *    eBADOBJECTID_OM
 *    some errors caused by function calls
 *
 * Side effect:
 *  1) parameter nextOID
 *     nextOID is filled with the next object's identifier
 *  2) parameter objHdr
 *     objHdr is filled with the next object's header
 */
Four EduOM_NextObject(
    ObjectID  *catObjForFile,	/* IN informations about a data file */
    ObjectID  *curOID,		/* IN a ObjectID of the current Object */
    ObjectID  *nextOID,		/* OUT the next Object of a current Object */
    ObjectHdr *objHdr)		/* OUT the object header of next object */
{
    Four e;			/* error */
    Two  i;			/* index */
    Four offset;		/* starting offset of object within a page */
    PageID pid;			/* a page identifier */
    PageNo pageNo;		/* a temporary var for next page's PageNo */
    SlottedPage *apage;		/* a pointer to the data page */
    Object *obj;		/* a pointer to the Object */
    PhysicalFileID pFid;	/* file in which the objects are located */
    SlottedPage *catPage;	/* buffer page containing the catalog object */
    sm_CatOverlayForData *catEntry; /* data structure for catalog object access */



    /*@
     * parameter checking
     */
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);
    
    if (nextOID == NULL) ERR(eBADOBJECTID_OM);

    e = BfM_GetTrain((TrainID*)catObjForFile, (char**)&catPage, PAGE_BUF);
    if (e < 0) ERR(e);
    catEntry = (sm_CatOverlayForData*)(catPage->data + catPage->slot[-catObjForFile->slotNo].offset);

    // Case 1: curOID is NULL → find first object in file
    if (!curOID) {
        pid.volNo = catEntry->fid.volNo;
        pid.pageNo = catEntry->firstPage;
    }else {
        pid.volNo = curOID->volNo;
        pid.pageNo = curOID->pageNo;
    }

    while(1){
        if (e = BfM_GetTrain(&pid, (char**)&apage, PAGE_BUF) < 0) ERR(e);
        Two startSlot = (curOID)? (curOID->slotNo + 1) : 0; //go to next page also
        for(i = startSlot; i < apage->header.nSlots; i++){
            if (apage->slot[-i].offset == EMPTYSLOT) continue;
            nextOID->pageNo = pid.pageNo;
            nextOID->volNo = pid.volNo;
            nextOID->slotNo = i;
            nextOID->unique = apage->slot[i].unique;
            if (objHdr) {
                ObjectHdr *hdr = (ObjectHdr*)(apage->data + apage->slot[-i].offset);
                memcpy(objHdr, hdr, sizeof(ObjectHdr));
            }
            BfM_FreeTrain(&pid, PAGE_BUF);
            BfM_FreeTrain((TrainID*)catObjForFile, PAGE_BUF);
            return eNOERROR;
        }
        if (pid.pageNo == catEntry->lastPage) {
            BfM_FreeTrain((TrainID *) catObjForFile, PAGE_BUF);
            return (EOS);
        }
        PageNo nextPage = apage->header.nextPage;
        BfM_FreeTrain(&pid, PAGE_BUF);
        pid.pageNo = nextPage;
        curOID = NULL;
    }
    /* end of scan */
    
} /* EduOM_NextObject() */
