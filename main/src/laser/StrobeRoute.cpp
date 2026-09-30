#include <PinConfig.h>
#include "StrobeRoute.h"
#include "LaserStrobe.h"
#include "Arduino.h"
#include "../canopen/RoutingTable.h"
#ifdef CAN_CONTROLLER_CANOPEN
#include "../canopen/CANopenModule.h"
#include "../canopen/UC2_OD_Indices.h"
#endif

namespace StrobeRoute
{
    static const UC2::RouteEntry *route(int laserId)
    {
        if (laserId < 0 || laserId > 255)
            return nullptr;
        return UC2::RoutingTable::find(UC2::RouteEntry::LASER, (uint8_t)laserId);
    }

    bool isRemote(int laserId)
    {
        const UC2::RouteEntry *r = route(laserId);
        return r && r->where == UC2::RouteEntry::REMOTE;
    }

    static Result localResult(int laserId)
    {
        Result res;
        res.supported = true;
        res.remote = false;
        res.enabled = LaserStrobe::isStrobing(laserId);
        res.delayUs = LaserStrobe::delayUs(laserId);
        res.widthUs = LaserStrobe::widthUs(laserId);
        res.count = LaserStrobe::fired(laserId);
        return res;
    }

#ifdef CAN_CONTROLLER_CANOPEN
    static bool readU8(uint8_t node, uint16_t idx, uint8_t sub, uint8_t &v)
    {
        size_t n = 0;
        return CANopenModule::readSDO(node, idx, sub, &v, sizeof(v), &n) && n >= 1;
    }

    static bool readU32(uint8_t node, uint16_t idx, uint8_t sub, uint32_t &v)
    {
        uint8_t b[4] = {0, 0, 0, 0};
        size_t n = 0;
        if (!CANopenModule::readSDO(node, idx, sub, b, sizeof(b), &n) || n < 4)
            return false;
        v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
        return true;
    }

    // Probe with a read of 0x2107: an old node aborts (object missing) but
    // stays reachable; a node that does not answer at all is unreachable.
    static bool probe(uint8_t node, uint8_t sub, Result &res)
    {
        uint8_t en = 0;
        if (readU8(node, UC2_OD::LASER_STROBE_ENABLE, sub, en))
        {
            res.supported = true;
            res.enabled = en != 0;
            return true;
        }
        res.error = CANopenModule::isNodeReachable(node)
                        ? "laser node has no strobe support (firmware too old)"
                        : "laser node unreachable";
        return false;
    }

    static void readBack(uint8_t node, uint8_t sub, Result &res)
    {
        uint8_t en = 0;
        if (readU8(node, UC2_OD::LASER_STROBE_ENABLE, sub, en))
            res.enabled = en != 0;
        readU32(node, UC2_OD::LASER_STROBE_DELAY_US, sub, res.delayUs);
        readU32(node, UC2_OD::LASER_STROBE_WIDTH_US, sub, res.widthUs);
        readU32(node, UC2_OD::LASER_STROBE_COUNT, sub, res.count);
    }
#endif

    Result configure(int laserId, bool enable, uint32_t delayUs, uint32_t widthUs)
    {
        Result res;
        const UC2::RouteEntry *r = route(laserId);
        if (!r || r->where == UC2::RouteEntry::OFF)
        {
            res.error = "laser id has no route";
            return res;
        }

        if (r->where == UC2::RouteEntry::LOCAL)
        {
            const char *err = nullptr;
            const bool ok = LaserStrobe::configure(laserId, enable, delayUs, widthUs, &err);
            res = localResult(laserId);
            res.ok = ok;
            res.error = err;
            return res;
        }

#ifdef CAN_CONTROLLER_CANOPEN
        res.remote = true;
        res.nodeId = r->nodeId;
        const uint8_t sub = (uint8_t)(r->subAxis + 1);
        if (!probe(r->nodeId, sub, res))
            return res;

        if (enable)
        {
            // Delay and width first so the node never strobes with stale values.
            CANopenModule::writeSDO_u32(r->nodeId, UC2_OD::LASER_STROBE_DELAY_US, sub, delayUs);
            CANopenModule::writeSDO_u32(r->nodeId, UC2_OD::LASER_STROBE_WIDTH_US, sub, widthUs);
            CANopenModule::writeSDO_u8(r->nodeId, UC2_OD::LASER_STROBE_ENABLE, sub, 1);
        }
        else
        {
            CANopenModule::writeSDO_u8(r->nodeId, UC2_OD::LASER_STROBE_ENABLE, sub, 0);
        }
        // The node applies the request from its main loop and writes the
        // result (0 on refusal, clamped values) back into its OD.
        vTaskDelay(pdMS_TO_TICKS(60));
        readBack(r->nodeId, sub, res);
        if (res.enabled != enable)
        {
            res.error = enable ? "laser node refused the strobe (no pin on this channel, or another channel is strobing)"
                               : "laser node did not switch the strobe off";
            return res;
        }
        res.ok = true;
        return res;
#else
        res.error = "remote laser needs CANopen";
        return res;
#endif
    }

    Result status(int laserId)
    {
        Result res;
        const UC2::RouteEntry *r = route(laserId);
        if (!r || r->where == UC2::RouteEntry::OFF)
        {
            res.error = "laser id has no route";
            return res;
        }
        if (r->where == UC2::RouteEntry::LOCAL)
        {
            res = localResult(laserId);
            res.ok = true;
            return res;
        }
#ifdef CAN_CONTROLLER_CANOPEN
        res.remote = true;
        res.nodeId = r->nodeId;
        const uint8_t sub = (uint8_t)(r->subAxis + 1);
        if (!probe(r->nodeId, sub, res))
            return res;
        readBack(r->nodeId, sub, res);
        res.ok = true;
#else
        res.error = "remote laser needs CANopen";
#endif
        return res;
    }

    cJSON *toJson(const Result &r)
    {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "supported", r.supported ? 1 : 0);
        cJSON_AddNumberToObject(o, "enabled", r.enabled ? 1 : 0);
        cJSON_AddNumberToObject(o, "remote", r.remote ? 1 : 0);
        if (r.remote)
            cJSON_AddNumberToObject(o, "node", r.nodeId);
        cJSON_AddNumberToObject(o, "delayUs", (double)r.delayUs);
        cJSON_AddNumberToObject(o, "widthUs", (double)r.widthUs);
        cJSON_AddNumberToObject(o, "count", (double)r.count);
        return o;
    }
}
