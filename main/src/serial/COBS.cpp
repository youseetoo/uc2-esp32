#include "COBS.h"

namespace COBS
{
    size_t encode(const uint8_t *src, size_t len, uint8_t *dst)
    {
        size_t readIdx = 0;
        size_t writeIdx = 1; // first byte is the code byte
        size_t codeIdx = 0;
        uint8_t code = 1;

        while (readIdx < len)
        {
            if (src[readIdx] == 0x00)
            {
                dst[codeIdx] = code;
                code = 1;
                codeIdx = writeIdx++;
                readIdx++;
            }
            else
            {
                dst[writeIdx++] = src[readIdx++];
                code++;
                if (code == 0xFF)
                {
                    dst[codeIdx] = code;
                    code = 1;
                    codeIdx = writeIdx++;
                }
            }
        }
        dst[codeIdx] = code;
        return writeIdx;
    }

    size_t decode(const uint8_t *src, size_t len, uint8_t *dst)
    {
        if (len == 0)
            return 0;

        size_t readIdx = 0;
        size_t writeIdx = 0;

        while (readIdx < len)
        {
            uint8_t code = src[readIdx++];
            if (code == 0)
                return 0; // unexpected zero in COBS block

            for (uint8_t i = 1; i < code; i++)
            {
                if (readIdx >= len)
                    return 0; // truncated
                dst[writeIdx++] = src[readIdx++];
            }

            if (code < 0xFF && readIdx < len)
            {
                dst[writeIdx++] = 0x00;
            }
        }

        // The last group should not add a trailing zero
        if (writeIdx > 0 && dst[writeIdx - 1] == 0x00)
            writeIdx--;

        return writeIdx;
    }
}
