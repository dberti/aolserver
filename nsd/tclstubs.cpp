/*
 * The contents of this file are subject to the AOLserver Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://aolserver.com/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is AOLserver Code and related documentation
 * distributed by AOL.
 * 
 * The Initial Developer of the Original Code is America Online,
 * Inc. Portions created by AOL are Copyright (C) 1999 America Online,
 * Inc. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the GNU General Public License (the "GPL"), in which case the
 * provisions of GPL are applicable instead of those above.  If you wish
 * to allow use of your version of this file only under the terms of the
 * GPL and not to allow others to use your version of this file under the
 * License, indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by the GPL.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under either the License or the GPL.
 */

/* 
 * tclstubs.cpp --
 *
 *	This file provides extensions to the Tcl 7.6 and Tcl 8.2 core
 *	required by AOLserver.  This file is conditionally compiled
 *      for the Tcl 7.6 and 8.2 versions of AOLserver.
 *
 */


#include	"nsd.h"

typedef struct Cmd {
    struct Cmd *nextPtr;
    Tcl_CmdInfo info;
    char name[1];
} Cmd;

#if TCL_MAJOR_VERSION >= 8

/*
 * The following structure defines a block in an ADP as either
 * text or compiled byte code object.
 */

typedef struct Block {
    struct Block *nextPtr;
    Tcl_Obj      *scriptObjPtr;
    int           length;
    char          text[4];
} Block;

static Tcl_Encoding GetEncoding(char *file);
static Tcl_HashTable extensions;
static Tcl_HashTable encodings;
static Ns_Mutex lock;

#endif

char *nsTclVersion = TCL_VERSION;


void
NsTclFinalizeThread(void)
{
#if TCL_MAJOR_VERSION >= 8
   Tcl_FinalizeThread();
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclFindExecutable --
 *
 *      Returns the name of the server executable.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
NsTclFindExecutable(char *argv0)
{
#if TCL_MAJOR_VERSION < 8
    extern char *tclExecutableName;
#endif
    static char *nsd;

    Tcl_FindExecutable(argv0);
    if (nsd != NULL) {
	ns_free(nsd);
    }
#if TCL_MAJOR_VERSION < 8
    nsd = tclExecutableName;
#else
    nsd = (char *) Tcl_GetNameOfExecutable();
#endif
    if (nsd != NULL) {
	nsd = ns_strdup(nsd);
    }
    return nsd;
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_TclGetOpenFd --
 *
 *	Return an open Unix file descriptor for the given channel.
 *	This routine is used by the AOLserver ns_sock* routines
 *	to provide access to the underlying socket.
 *
 * Results:
 *	TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *	The value at fdPtr is updated with a valid Unix file descriptor.
 *
 *----------------------------------------------------------------------
 */

int
Ns_TclGetOpenFd(Tcl_Interp *interp, char *chanId, int write, int *fdPtr)
{
    Tcl_Channel chan;
#if TCL_MAJOR_VERSION >= 8
    ClientData data;
#else
    Tcl_File file;
#endif

    if (Ns_TclGetOpenChannel(interp, chanId, write, 1, &chan) != TCL_OK) {
	return TCL_ERROR;
    }
#if TCL_MAJOR_VERSION >= 8
    if (Tcl_GetChannelHandle(chan, write ? TCL_WRITABLE : TCL_READABLE,
			     (ClientData*) &data) != TCL_OK) {
	Tcl_AppendResult(interp, "could not get handle for channel: ",
			 chanId, NULL);
	return TCL_ERROR;
    }
    *fdPtr = (int) data;
#else
    file = Tcl_GetChannelFile(chan, write ? TCL_WRITABLE : TCL_READABLE);
    if (file == NULL) {
	Tcl_AppendResult(interp, "could not get file for channel: ", chanId,
		         NULL);
	return TCL_ERROR;
    }
    *fdPtr = (int) Tcl_GetFileInfo(file, NULL);
#endif

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclEval --
 *
 *      Wraps Tcl_Eval() of Tcl_EvalEx() to avoid byte code compiling.
 *
 * Results:
 *      See Tcl_Eval
 *
 * Side effects:
 *	See Tcl_Eval
 *
 *----------------------------------------------------------------------
 */

int
NsTclEval(Tcl_Interp *interp, char *script)
{
    int status;

#if TCL_MAJOR_VERSION >= 8
    /* 
     * Eval without the byte code compiler, and ensure that we
     * have a string result so old code can reference interp->result.
     */

    status = Tcl_EvalEx(interp, script, strlen(script), TCL_EVAL_DIRECT);
    Tcl_SetResult(interp, Tcl_GetString(Tcl_GetObjResult(interp)),
		  TCL_VOLATILE);
#else
    status = Tcl_Eval(interp, script);
#endif
    return status;
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclGetCmdInfo
 *
 *      Allocates a version-independent Cmd structure with
 *  	version-dependent command info.  This code must
 *  	be compiled here because the Tcl_CmdInfo structure
 *  	changed size between 7.6 and 8.2.
 *
 * Results:
 *      Opaque pointer to Cmd structure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TclCmdInfo *
NsTclGetCmdInfo(Tcl_Interp *interp, char *cmdName)
{
    Cmd *cmdPtr;
    
    cmdPtr = ns_malloc(sizeof(struct Cmd) + strlen(cmdName));
    strcpy(cmdPtr->name, cmdName);
    cmdPtr->nextPtr = NULL;
    Tcl_GetCommandInfo(interp, cmdPtr->name, &cmdPtr->info);
    return (TclCmdInfo *) cmdPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclCreateCommand --
 *
 *      Create a command defined by a version-independent Cmd structure.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
NsTclCreateCommand(Tcl_Interp *interp, TclCmdInfo *info)
{
    Cmd *cmdPtr = (Cmd *) info;
    
#if TCL_MAJOR_VERSION >= 8
    if (cmdPtr->info.isNativeObjectProc) {
	Tcl_CreateObjCommand(interp, cmdPtr->name, cmdPtr->info.objProc,
	    cmdPtr->info.objClientData, cmdPtr->info.deleteProc);
    } else {
	Tcl_CreateCommand(interp, cmdPtr->name, cmdPtr->info.proc,
	    cmdPtr->info.clientData, cmdPtr->info.deleteProc);
    }
#else
    Tcl_CreateCommand(interp, cmdPtr->name, cmdPtr->info.proc,
	    cmdPtr->info.clientData, cmdPtr->info.deleteProc);
#endif

    /*
     * NB: A call to Tcl_SetCommandInfo is required after 
     * Tcl_Create{Obj}Command above to ensure the delete
     * data is set correctly.
     */

    Tcl_SetCommandInfo(interp, cmdPtr->name, &cmdPtr->info);
}


/*
 *----------------------------------------------------------------------
 *
 * NsAdpCopyPrivate --
 *
 *  	Create a private, version-specific page.
 *
 * Results:
 *	A pointer to new pagePtr (must be free'd
 *  	with NsAdpFreePrivate).
 *
 * Side effects:
 *  	None.
 *
 *----------------------------------------------------------------------
 */

Page *
NsAdpCopyPrivate(Ns_DString *dsPtr, struct stat *stPtr)
{
#if TCL_MAJOR_VERSION < 8
    /*
     * Format is identical to shared for 7.6.
     */
     
    return NsAdpCopyShared(dsPtr, stPtr);
#else
    Block *blockPtr, *prevPtr;
    register char *t, cmd;
    int n;
    Page *pagePtr;

    /*
     * Convert the chunky format into a list of blocks for 8.2.
     */

    pagePtr = ns_calloc(1, sizeof(Page));
    pagePtr->mtime = stPtr->st_mtime;
    pagePtr->size = stPtr->st_size;
    t = dsPtr->string;
    while (*t) {
	cmd = *t++;
	n = strlen(t);
	if (cmd == 't') {
	    blockPtr = ns_malloc(sizeof(Block) + n);
	    blockPtr->scriptObjPtr = NULL;
	    blockPtr->length = n;
	    memcpy(blockPtr->text, t, n);
	} else {
	    blockPtr = ns_malloc(sizeof(Block));
	    blockPtr->length = 0;
	    blockPtr->scriptObjPtr = Tcl_NewStringObj(t, n);
	    Tcl_IncrRefCount(blockPtr->scriptObjPtr);
	}
	blockPtr->nextPtr = NULL;
	if (pagePtr->pdPtr == NULL) {
	    pagePtr->pdPtr = blockPtr;
	} else {
	    prevPtr->nextPtr = blockPtr;
	}
	prevPtr = blockPtr;
	t += n+1;
    }
    return pagePtr;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * NsAdpFreePrivate --
 *
 *  	Free previously allocated private page.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
NsAdpFreePrivate(Page *pagePtr)
{
#if TCL_MAJOR_VERSION >= 8
    Block *blockPtr = (Block *) pagePtr->pdPtr;
    Block *nextBlockPtr;

    while (blockPtr != NULL) {
	nextBlockPtr = blockPtr->nextPtr;
	if (blockPtr->scriptObjPtr != NULL) {
	    Tcl_DecrRefCount(blockPtr->scriptObjPtr);
	}
	ns_free(blockPtr);
	blockPtr = nextBlockPtr;
    }
#endif
    ns_free(pagePtr);
}


/*
 *----------------------------------------------------------------------
 *
 * NsAdpRunPrivate --
 *
 *	This evaluates the private copy from the DString,
 *	which was previously copied out of the cache by
 *	NsAdpCopyPrivate.
 *
 * Results:
 *	A Tcl status code from NsAdpEval
 *
 * Side Effects:
 *	Whatever NsAdpEval does.
 *
 *----------------------------------------------------------------------
 */

int
NsAdpRunPrivate(Tcl_Interp *interp, char *file, Page *pagePtr)
{
#if TCL_MAJOR_VERSION < 8
    /*
     * Evaluate directly for 7.6.
     */
     
    return NsAdpEval(interp, file, pagePtr->chunks);
#else
    Block *blockPtr = (Block *) pagePtr->pdPtr;
    int chunk, code;
    AdpData *adPtr = NsAdpGetData();

    chunk = 0;
    code = TCL_OK;
    while (blockPtr != NULL && adPtr->exception == ADP_OK) {
	if (blockPtr->scriptObjPtr == NULL) {
	    Ns_DStringNAppend(&adPtr->output, blockPtr->text, blockPtr->length);
	} else {
    	    code = Tcl_EvalObjEx(interp, blockPtr->scriptObjPtr, 0);
	    if (code != TCL_OK && code != TCL_RETURN && adPtr->exception == ADP_OK) {
    	    	NsAdpLogError(interp, file, chunk);
	    }
	    ++chunk;
	}
	blockPtr = blockPtr->nextPtr;
	NsAdpFlush(adPtr);
    }


    /*
     * Tcl8x: ADP_RETURN should not unroll all the callers.
     */
    if (adPtr->exception == ADP_RETURN) {
	adPtr->exception = ADP_OK;
	code = TCL_OK;
    }

    NsAdpFlush(adPtr);

    return code;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * NsExt2Utf --
 *
 *	Encode to UTF.
 *
 * Results:
 *	Pointer to output.
 *
 * Side Effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
NsExt2Utf(void *enc, char *ext, Tcl_DString *dsPtr)
{
#if TCL_MAJOR_VERSION < 8
    return ext;
#else
    if (enc == NULL) {
	return ext;
    }
    Tcl_ExternalToUtfDString((Tcl_Encoding) enc, ext, strlen(ext), dsPtr);
    return dsPtr->string;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * NsUtf2Ext --
 *
 *	Decode from UTF.
 *
 * Results:
 *	Pointer to output.
 *
 * Side Effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
NsUtf2Ext(void *enc, char *utf, Tcl_DString *dsPtr)
{
#if TCL_MAJOR_VERSION < 8
    return utf;
#else
    if (enc == NULL) {
	return utf;
    }
    Tcl_UtfToExternalDString((Tcl_Encoding) enc, utf, strlen(utf), dsPtr);
    return dsPtr->string;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * NsInitEncodings --
 *
 *	Initialize the file encodings table.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Tcl_Encodings will be loaded when first needed.
 *
 *----------------------------------------------------------------------
 */

void
NsInitEncodings(void)
{
#if TCL_MAJOR_VERSION >= 8
    Tcl_HashEntry *hPtr;
    char *path, *ext, *enc;
    Ns_Set *set;
    int i, new;

    Ns_MutexSetName(&lock, "ns:encodings");
    Tcl_InitHashTable(&extensions, TCL_STRING_KEYS);
    Tcl_InitHashTable(&encodings, TCL_STRING_KEYS);
    set = Ns_ConfigGetSection("ns/encodings");
    for (i = 0; set != NULL && i < Ns_SetSize(set); ++i) {
	ext = Ns_SetKey(set, i);
	enc = Ns_SetValue(set, i);
	hPtr = Tcl_CreateHashEntry(&extensions, ext, &new);
	Tcl_SetHashValue(hPtr, enc);
    }
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * NsGetEnc --
 *
 *	Get the encoding by Tcl name.
 *
 * Results:
 *	Void poitner to Tcl_Encoding or NULL not loadable.
 *
 * Side Effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void *
NsGetEnc(char *name)
{
#if TCL_MAJOR_VERSION < 8
    return NULL;
#else
    Tcl_HashEntry *hPtr;
    Tcl_Encoding encoding;
    int new;

    Ns_MutexLock(&lock);
    hPtr = Tcl_CreateHashEntry(&encodings, name, &new);
    if (!new) {
    	encoding = Tcl_GetHashValue(hPtr);
    } else {
	encoding = Tcl_GetEncoding(NULL, name);
	if (encoding != NULL) {
	    Ns_Log(Notice, "loaded encoding: %s", name);
	} else {
	    Ns_Log(Error, "could not load encoding: %s", name);
	}
	Tcl_SetHashValue(hPtr, encoding);
    }
    Ns_MutexUnlock(&lock);
    return (void *) encoding;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * NsGetFileEnc --
 *
 *	Get the encoding registered for a file.
 *
 * Results:
 *	Void poitner to Tcl_Encoding or NULL if unknown or not loadable.
 *
 * Side Effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void *
NsGetFileEnc(char *file)
{
#if TCL_MAJOR_VERSION >= 8
    Tcl_HashEntry *hPtr;
    char *ext;

    ext = strrchr(file, '.');
    if (ext != NULL) {
	hPtr = Tcl_FindHashEntry(&extensions, ext+1);
	if (hPtr != NULL) {
	    return NsGetEnc(Tcl_GetHashValue(hPtr));
	}
    }
#endif
    return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * NsGetEncName --
 *
 *	Return the name of the encoding.
 *
 * Results:
 *	Pointer to encoding name or NULL.
 *
 * Side Effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
NsGetEncName(void *enc)
{
#if TCL_MAJOR_VERSION >= 8
    if (enc != NULL) {
	return Tcl_GetEncodingName((Tcl_Encoding) enc);
    }
#endif
    return NULL;
}