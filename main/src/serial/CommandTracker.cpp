#include "CommandTracker.h"
#include "SerialTransport.h"
#include "SerialProcess.h"
#include "cJSON.h"
#include <cstring>

namespace CommandTracker
{

    // Helper: build a base response object with qid and status
    static cJSON *makeBase(int qid, const char *status)
    {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "qid", qid);
        cJSON_AddStringToObject(resp, "status", status);
        return resp;
    }

    // Helper: merge fields from src into dst (shallow copy of each item)
    static void mergeFields(cJSON *dst, const cJSON *src)
    {
        if (!src)
            return;
        const cJSON *item = nullptr;
        cJSON_ArrayForEach(item, src)
        {
            // Skip qid and status – we already set those
            if (strcmp(item->string, "qid") == 0 || strcmp(item->string, "status") == 0)
                continue;
            cJSON_AddItemToObject(dst, item->string, cJSON_Duplicate(item, true));
        }
    }

    // Helper: send a cJSON response using the thread-safe output path
    static void sendResponse(cJSON *resp, TransportFormat format)
    {
        if (resp == NULL)
            return;
        char *s = cJSON_PrintUnformatted(resp);
        if (s == NULL)
            return;
        SerialProcess::safeSendJsonString(s);
        free(s);
    }

    void sendACK(int qid, TransportFormat format)
    {
        cJSON *resp = makeBase(qid, "ACK");
        sendResponse(resp, format);
        cJSON_Delete(resp);
    }

    void sendDONE(int qid, TransportFormat format, cJSON *resultData)
    {
        cJSON *resp = makeBase(qid, "DONE");
        mergeFields(resp, resultData);
        sendResponse(resp, format);
        cJSON_Delete(resp);
    }

    void sendERROR(int qid, TransportFormat format, const char *errorMsg)
    {
        cJSON *resp = makeBase(qid, "ERROR");
        if (errorMsg)
            cJSON_AddStringToObject(resp, "error", errorMsg);
        sendResponse(resp, format);
        cJSON_Delete(resp);
    }

    void sendImmediate(int qid, TransportFormat format, cJSON *resultData)
    {
        // Combined ACK+DONE in one response
        cJSON *resp = makeBase(qid, "DONE");
        mergeFields(resp, resultData);
        sendResponse(resp, format);
        cJSON_Delete(resp);
    }

} // namespace CommandTracker
