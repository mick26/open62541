/* This Source Code Form is subject to the terms of the Mozilla Public
*  License, v. 2.0. If a copy of the MPL was not distributed with this 
*  file, You can obtain one at http://mozilla.org/MPL/2.0/.*/

#include "ua_client.h"
#include "ua_client_highlevel.h"
#include "ua_util.h"
#include "ua_nodeids.h"

UA_StatusCode
UA_Client_NamespaceGetIndex(UA_Client *client, UA_String *namespaceUri,
                            UA_UInt16 *namespaceIndex) {
    UA_ReadRequest request;
    UA_ReadRequest_init(&request);
    UA_ReadValueId id;
    id.attributeId = UA_ATTRIBUTEID_VALUE;
    id.nodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_NAMESPACEARRAY);
    request.nodesToRead = &id;
    request.nodesToReadSize = 1;

    UA_ReadResponse response = UA_Client_Service_read(client, request);

    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    if(response.responseHeader.serviceResult != UA_STATUSCODE_GOOD)
        retval = response.responseHeader.serviceResult;
    else if(response.resultsSize != 1 || !response.results[0].hasValue)
        retval = UA_STATUSCODE_BADNODEATTRIBUTESINVALID;
    else if(response.results[0].value.type != &UA_TYPES[UA_TYPES_STRING])
        retval = UA_STATUSCODE_BADTYPEMISMATCH;

    if(retval != UA_STATUSCODE_GOOD) {
        UA_ReadResponse_deleteMembers(&response);
        return retval;
    }

    retval = UA_STATUSCODE_BADNOTFOUND;
    UA_String *ns = response.results[0].value.data;
    for(size_t i = 0; i < response.results[0].value.arrayLength; ++i){
        if(UA_String_equal(namespaceUri, &ns[i])) {
            *namespaceIndex = (UA_UInt16)i;
            retval = UA_STATUSCODE_GOOD;
            break;
        }
    }

    UA_ReadResponse_deleteMembers(&response);
    return retval;
}

UA_StatusCode
UA_Client_forEachChildNodeCall(UA_Client *client, UA_NodeId parentNodeId,
                               UA_NodeIteratorCallback callback, void *handle) {
  UA_BrowseRequest bReq;
  UA_BrowseRequest_init(&bReq);
  bReq.requestedMaxReferencesPerNode = 0;
  bReq.nodesToBrowse = UA_BrowseDescription_new();
  bReq.nodesToBrowseSize = 1;
  UA_NodeId_copy(&parentNodeId, &bReq.nodesToBrowse[0].nodeId);
  bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL; //return everything
  bReq.nodesToBrowse[0].browseDirection = UA_BROWSEDIRECTION_BOTH;
  
  UA_BrowseResponse bResp = UA_Client_Service_browse(client, bReq);
  
  UA_StatusCode retval = UA_STATUSCODE_GOOD;
  if(bResp.responseHeader.serviceResult == UA_STATUSCODE_GOOD) {
      for (size_t i = 0; i < bResp.resultsSize; ++i) {
          for (size_t j = 0; j < bResp.results[i].referencesSize; ++j) {
              UA_ReferenceDescription *ref = &(bResp.results[i].references[j]);
              retval |= callback(ref->nodeId.nodeId, !ref->isForward,
                                 ref->referenceTypeId, handle);
          }
      }
  }
  else
      retval = bResp.responseHeader.serviceResult;
  
  
  UA_BrowseRequest_deleteMembers(&bReq);
  UA_BrowseResponse_deleteMembers(&bResp);
  
  return retval;
}

/*******************/
/* Node Management */
/*******************/

UA_StatusCode UA_EXPORT
UA_Client_addReference(UA_Client *client, const UA_NodeId sourceNodeId,
                       const UA_NodeId referenceTypeId, UA_Boolean isForward,
                       const UA_String targetServerUri,
                       const UA_ExpandedNodeId targetNodeId,
                       UA_NodeClass targetNodeClass) {
    UA_AddReferencesItem item;
    UA_AddReferencesItem_init(&item);
    item.sourceNodeId = sourceNodeId;
    item.referenceTypeId = referenceTypeId;
    item.isForward = isForward;
    item.targetServerUri = targetServerUri;
    item.targetNodeId = targetNodeId;
    item.targetNodeClass = targetNodeClass;
    UA_AddReferencesRequest request;
    UA_AddReferencesRequest_init(&request);
    request.referencesToAdd = &item;
    request.referencesToAddSize = 1;
    UA_AddReferencesResponse response = UA_Client_Service_addReferences(client, request);
    UA_StatusCode retval = response.responseHeader.serviceResult;
    if(retval != UA_STATUSCODE_GOOD) {
        UA_AddReferencesResponse_deleteMembers(&response);
        return retval;
    }
    if(response.resultsSize != 1) {
        UA_AddReferencesResponse_deleteMembers(&response);
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    retval = response.results[0];
    UA_AddReferencesResponse_deleteMembers(&response);
    return retval;
}

UA_StatusCode UA_EXPORT
UA_Client_deleteReference(UA_Client *client, const UA_NodeId sourceNodeId,
                          const UA_NodeId referenceTypeId, UA_Boolean isForward,
                          const UA_ExpandedNodeId targetNodeId,
                          UA_Boolean deleteBidirectional) {
    UA_DeleteReferencesItem item;
    UA_DeleteReferencesItem_init(&item);
    item.sourceNodeId = sourceNodeId;
    item.referenceTypeId = referenceTypeId;
    item.isForward = isForward;
    item.targetNodeId = targetNodeId;
    item.deleteBidirectional = deleteBidirectional;
    UA_DeleteReferencesRequest request;
    UA_DeleteReferencesRequest_init(&request);
    request.referencesToDelete = &item;
    request.referencesToDeleteSize = 1;
    UA_DeleteReferencesResponse response = UA_Client_Service_deleteReferences(client, request);
    UA_StatusCode retval = response.responseHeader.serviceResult;
    if(retval != UA_STATUSCODE_GOOD) {
        UA_DeleteReferencesResponse_deleteMembers(&response);
        return retval;
    }
    if(response.resultsSize != 1) {
        UA_DeleteReferencesResponse_deleteMembers(&response);
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    retval = response.results[0];
    UA_DeleteReferencesResponse_deleteMembers(&response);
    return retval;
}

UA_StatusCode
UA_Client_deleteNode(UA_Client *client, const UA_NodeId nodeId,
                     UA_Boolean deleteTargetReferences) {
    UA_DeleteNodesItem item;
    UA_DeleteNodesItem_init(&item);
    item.nodeId = nodeId;
    item.deleteTargetReferences = deleteTargetReferences;
    UA_DeleteNodesRequest request;
    UA_DeleteNodesRequest_init(&request);
    request.nodesToDelete = &item;
    request.nodesToDeleteSize = 1;
    UA_DeleteNodesResponse response = UA_Client_Service_deleteNodes(client, request);
    UA_StatusCode retval = response.responseHeader.serviceResult;
    if(retval != UA_STATUSCODE_GOOD) {
        UA_DeleteNodesResponse_deleteMembers(&response);
        return retval;
    }
    if(response.resultsSize != 1) {
        UA_DeleteNodesResponse_deleteMembers(&response);
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    retval = response.results[0];
    UA_DeleteNodesResponse_deleteMembers(&response);
    return retval;
}

UA_StatusCode
__UA_Client_addNode(UA_Client *client, const UA_NodeClass nodeClass,
                    const UA_NodeId requestedNewNodeId, const UA_NodeId parentNodeId,
                    const UA_NodeId referenceTypeId, const UA_QualifiedName browseName,
                    const UA_NodeId typeDefinition, const UA_NodeAttributes *attr,
                    const UA_DataType *attributeType, UA_NodeId *outNewNodeId) {
    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    UA_AddNodesRequest request;
    UA_AddNodesRequest_init(&request);
    UA_AddNodesItem item;
    UA_AddNodesItem_init(&item);
    item.parentNodeId.nodeId = parentNodeId;
    item.referenceTypeId = referenceTypeId;
    item.requestedNewNodeId.nodeId = requestedNewNodeId;
    item.browseName = browseName;
    item.nodeClass = nodeClass;
    item.typeDefinition.nodeId = typeDefinition;
    item.nodeAttributes.encoding = UA_EXTENSIONOBJECT_DECODED_NODELETE;
    item.nodeAttributes.content.decoded.type = attributeType;
    item.nodeAttributes.content.decoded.data = (void*)(uintptr_t)attr; // hack. is not written into.
    request.nodesToAdd = &item;
    request.nodesToAddSize = 1;
    UA_AddNodesResponse response = UA_Client_Service_addNodes(client, request);
    if(response.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        retval = response.responseHeader.serviceResult;
        UA_AddNodesResponse_deleteMembers(&response);
        return retval;
    }
    if(response.resultsSize != 1) {
        UA_AddNodesResponse_deleteMembers(&response);
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if(outNewNodeId && response.results[0].statusCode == UA_STATUSCODE_GOOD) {
        *outNewNodeId = response.results[0].addedNodeId;
        UA_NodeId_init(&response.results[0].addedNodeId);
    }
    retval = response.results[0].statusCode;
    UA_AddNodesResponse_deleteMembers(&response);
    return retval;
}

/********/
/* Call */
/********/

#ifdef UA_ENABLE_METHODCALLS

UA_StatusCode
UA_Client_call(UA_Client *client, const UA_NodeId objectId,
               const UA_NodeId methodId, size_t inputSize,
               const UA_Variant *input, size_t *outputSize,
               UA_Variant **output) {
    UA_CallRequest request;
    UA_CallRequest_init(&request);
    UA_CallMethodRequest item;
    UA_CallMethodRequest_init(&item);
    item.methodId = methodId;
    item.objectId = objectId;
    item.inputArguments = (void*)(uintptr_t)input; // cast const...
    item.inputArgumentsSize = inputSize;
    request.methodsToCall = &item;
    request.methodsToCallSize = 1;
    UA_CallResponse response = UA_Client_Service_call(client, request);
    UA_StatusCode retval = response.responseHeader.serviceResult;
    if(retval != UA_STATUSCODE_GOOD) {
        UA_CallResponse_deleteMembers(&response);
        return retval;
    }
    if(response.resultsSize != 1) {
        UA_CallResponse_deleteMembers(&response);
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    retval = response.results[0].statusCode;
    if(retval == UA_STATUSCODE_GOOD && response.resultsSize > 0) {
        if (output != NULL && outputSize != NULL) {
          *output = response.results[0].outputArguments;
          *outputSize = response.results[0].outputArgumentsSize;
        }
        response.results[0].outputArguments = NULL;
        response.results[0].outputArgumentsSize = 0;
    }
    UA_CallResponse_deleteMembers(&response);
    return retval;
}

#endif

/********************/
/* Write Attributes */
/********************/

UA_StatusCode 
__UA_Client_writeAttribute(UA_Client *client, const UA_NodeId *nodeId,
                           UA_AttributeId attributeId, const void *in,
                           const UA_DataType *inDataType) {
    if(!in)
      return UA_STATUSCODE_BADTYPEMISMATCH;
    
    UA_WriteValue wValue;
    UA_WriteValue_init(&wValue);
    wValue.nodeId = *nodeId;
    wValue.attributeId = attributeId;
    if(attributeId == UA_ATTRIBUTEID_VALUE)
        wValue.value.value = *(const UA_Variant*)in;
    else
        /* hack. is never written into. */
        UA_Variant_setScalar(&wValue.value.value, (void*)(uintptr_t)in, inDataType);
    wValue.value.hasValue = true;
    UA_WriteRequest wReq;
    UA_WriteRequest_init(&wReq);
    wReq.nodesToWrite = &wValue;
    wReq.nodesToWriteSize = 1;
    
    UA_WriteResponse wResp = UA_Client_Service_write(client, wReq);
    UA_StatusCode retval = wResp.responseHeader.serviceResult;
    UA_WriteResponse_deleteMembers(&wResp);
    return retval;
}

UA_StatusCode
UA_Client_writeArrayDimensionsAttribute(UA_Client *client, const UA_NodeId nodeId,
                                        const UA_Int32 *newArrayDimensions,
                                        size_t newArrayDimensionsSize) {
    if(!newArrayDimensions)
      return UA_STATUSCODE_BADTYPEMISMATCH;
    
    UA_WriteValue wValue;
    UA_WriteValue_init(&wValue);
    wValue.nodeId = nodeId;
    wValue.attributeId = UA_ATTRIBUTEID_ARRAYDIMENSIONS;
    UA_Variant_setArray(&wValue.value.value, (void*)(uintptr_t)newArrayDimensions,
                        newArrayDimensionsSize, &UA_TYPES[UA_TYPES_INT32]);
    wValue.value.hasValue = true;
    UA_WriteRequest wReq;
    UA_WriteRequest_init(&wReq);
    wReq.nodesToWrite = &wValue;
    wReq.nodesToWriteSize = 1;
    
    UA_WriteResponse wResp = UA_Client_Service_write(client, wReq);
    UA_StatusCode retval = wResp.responseHeader.serviceResult;
    UA_WriteResponse_deleteMembers(&wResp);
    return retval;
}

/*******************/
/* Read Attributes */
/*******************/

UA_StatusCode 
__UA_Client_readAttribute(UA_Client *client, const UA_NodeId *nodeId,
                          UA_AttributeId attributeId, void *out,
                          const UA_DataType *outDataType) {
    UA_ReadValueId item;
    UA_ReadValueId_init(&item);
    item.nodeId = *nodeId;
    item.attributeId = attributeId;
    UA_ReadRequest request;
    UA_ReadRequest_init(&request);
    request.nodesToRead = &item;
    request.nodesToReadSize = 1;
    UA_ReadResponse response = UA_Client_Service_read(client, request);
    UA_StatusCode retval = response.responseHeader.serviceResult;
    if(retval == UA_STATUSCODE_GOOD && response.resultsSize != 1)
        retval = UA_STATUSCODE_BADUNEXPECTEDERROR;
    if(retval != UA_STATUSCODE_GOOD) {
        UA_ReadResponse_deleteMembers(&response);
        return retval;
    }

    /* Set the StatusCode */
    UA_DataValue *res = response.results;
    if(res->hasStatus)
        retval = res->status;

    /* Return early of no value is given */
    if(!res->hasValue) {
        if(retval == UA_STATUSCODE_GOOD)
            retval = UA_STATUSCODE_BADUNEXPECTEDERROR;
        UA_ReadResponse_deleteMembers(&response);
        return retval;
    }

    /* Copy value into out */
    if(attributeId == UA_ATTRIBUTEID_VALUE) {
        memcpy(out, &res->value, sizeof(UA_Variant));
        UA_Variant_init(&res->value);
    } else if(UA_Variant_isScalar(&res->value) &&
              res->value.type == outDataType) {
        memcpy(out, res->value.data, res->value.type->memSize);
        UA_free(res->value.data);
        res->value.data = NULL;
    } else {
        retval = UA_STATUSCODE_BADUNEXPECTEDERROR;
    }

    UA_ReadResponse_deleteMembers(&response);
    return retval;
}

UA_StatusCode
UA_Client_readArrayDimensionsAttribute(UA_Client *client, const UA_NodeId nodeId,
                                       UA_Int32 **outArrayDimensions,
                                       size_t *outArrayDimensionsSize) {
    UA_ReadValueId item;
    UA_ReadValueId_init(&item);
    item.nodeId = nodeId;
    item.attributeId = UA_ATTRIBUTEID_ARRAYDIMENSIONS;
    UA_ReadRequest request;
    UA_ReadRequest_init(&request);
    request.nodesToRead = &item;
    request.nodesToReadSize = 1;
    UA_ReadResponse response = UA_Client_Service_read(client, request);
    UA_StatusCode retval = response.responseHeader.serviceResult;
    if(retval == UA_STATUSCODE_GOOD && response.resultsSize != 1)
        retval = UA_STATUSCODE_BADUNEXPECTEDERROR;
    if(retval != UA_STATUSCODE_GOOD)
        goto cleanup;

    UA_DataValue *res = response.results;
    if(res->hasStatus != UA_STATUSCODE_GOOD)
        retval = res->hasStatus;
    else if(!res->hasValue || UA_Variant_isScalar(&res->value))
        retval = UA_STATUSCODE_BADUNEXPECTEDERROR;
    if(retval != UA_STATUSCODE_GOOD)
        goto cleanup;

    if(UA_Variant_isScalar(&res->value) ||
       res->value.type != &UA_TYPES[UA_TYPES_INT32]) {
        retval = UA_STATUSCODE_BADUNEXPECTEDERROR;
        goto cleanup;
    }

    *outArrayDimensions = res->value.data;
    *outArrayDimensionsSize = res->value.arrayLength;
    UA_free(res->value.data);
    res->value.data = NULL;
    res->value.arrayLength = 0;

 cleanup:
    UA_ReadResponse_deleteMembers(&response);
    return retval;

}
