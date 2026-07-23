#include "AxisCalibration.h"
#include <Preferences.h>
#include <math.h>
#include "esp_log.h"

static const char *TAG = "AxisCal";

namespace
{
    // Ordinary least-squares fit of y = slope*x + intercept over n points.
    // Also returns R^2 and the residual standard deviation (in y units).
    struct Regression
    {
        double slope = 0, intercept = 0, r2 = 0, residualStd = 0;
        bool ok = false;
    };

    Regression linearRegress(const double *x, const double *y, int n)
    {
        Regression r;
        if (n < 2)
            return r;

        double sx = 0, sy = 0, sxx = 0, sxy = 0, syy = 0;
        for (int i = 0; i < n; i++)
        {
            sx += x[i];
            sy += y[i];
            sxx += x[i] * x[i];
            sxy += x[i] * y[i];
            syy += y[i] * y[i];
        }
        double denom = n * sxx - sx * sx;
        if (fabs(denom) < 1e-9)
            return r;

        r.slope = (n * sxy - sx * sy) / denom;
        r.intercept = (sy - r.slope * sx) / n;

        // R^2 and residual scatter
        double ssRes = 0;
        for (int i = 0; i < n; i++)
        {
            double pred = r.slope * x[i] + r.intercept;
            double resid = y[i] - pred;
            ssRes += resid * resid;
        }
        double meanY = sy / n;
        double ssTot = 0;
        for (int i = 0; i < n; i++)
            ssTot += (y[i] - meanY) * (y[i] - meanY);

        r.r2 = (ssTot > 1e-9) ? (1.0 - ssRes / ssTot) : 0.0;
        r.residualStd = sqrt(ssRes / (double)(n > 2 ? n - 2 : 1));
        r.ok = true;
        return r;
    }
}

namespace AxisCalibrationRoutine
{
    // Move and, on return, report how many counts moved. Returns false on abort.
    static bool moveAndMeasure(const Hooks &h, const Params &p, int32_t deltaSteps,
                               int64_t &deltaCountsOut)
    {
        int64_t before = h.getRawCount();
        if (!h.moveRelBlocking(deltaSteps, p.probeSpeed))
            return false;
        delay(p.settleMs);
        deltaCountsOut = h.getRawCount() - before;
        return true;
    }

    bool run(const Hooks &hooks, const Params &params, AxisCalibration &out, AxisFault &err)
    {
        err = FAULT_NONE;
        out = AxisCalibration();
        out.microstepsAtCal = params.currentMicrosteps;

        if (!hooks.getStepPos || !hooks.moveRelBlocking || !hooks.getRawCount || !hooks.aborted)
        {
            err = FAULT_CAL_FAILED;
            ESP_LOGE(TAG, "Calibration hooks not fully wired");
            return false;
        }
        if (hooks.aborted())
        {
            err = FAULT_CAL_FAILED;
            ESP_LOGE(TAG, "Cannot calibrate: axis already in an abort state (endstop?)");
            return false;
        }

        // ---- Step 1: ORIGIN -------------------------------------------------
        // Zeroing of the encoder is the caller's job (it owns the backend); we
        // just record the common origin implicitly via delta measurements.
        ESP_LOGI(TAG, "Calibration start (microsteps=%u, probeSpeed=%d)",
                 params.currentMicrosteps, params.probeSpeed);

        // ---- Step 2: SIGN ---------------------------------------------------
        int64_t dPlus = 0, dMinus = 0;
        if (!moveAndMeasure(hooks, params, +params.lengths[0], dPlus))
        {
            err = FAULT_CAL_FAILED;
            ESP_LOGE(TAG, "SIGN: aborted during + probe");
            return false;
        }
        if (!moveAndMeasure(hooks, params, -params.lengths[0], dMinus))
        {
            err = FAULT_CAL_FAILED;
            ESP_LOGE(TAG, "SIGN: aborted during - probe");
            return false;
        }
        if (llabs(dPlus) < params.minCountsForValidity ||
            llabs(dMinus) < params.minCountsForValidity)
        {
            err = FAULT_CAL_FAILED;
            ESP_LOGE(TAG, "SIGN: encoder barely moved (d+=%lld d-=%lld) — dead/unwired?",
                     dPlus, dMinus);
            return false;
        }
        // + and - must produce opposite-sign count changes.
        if ((dPlus > 0) == (dMinus > 0))
        {
            err = FAULT_CAL_FAILED;
            ESP_LOGE(TAG, "SIGN: directions disagree (d+=%lld d-=%lld)", dPlus, dMinus);
            return false;
        }
        int8_t countSign = (dPlus > 0) ? 1 : -1;
        ESP_LOGI(TAG, "SIGN: countSign=%d (d+=%lld d-=%lld)", countSign, dPlus, dMinus);

        // ---- Step 3: SCALE by regression -----------------------------------
        // Command several lengths in BOTH directions; regress counts vs steps.
        // Using signed steps/counts anchors the intercept and averages slack.
        const int maxPts = 2 * 4;
        double xs[maxPts];
        double ys[maxPts];
        int n = 0;
        uint8_t nl = params.numLengths;
        if (nl > 4)
            nl = 4;

        for (uint8_t i = 0; i < nl && n < maxPts; i++)
        {
            int64_t dc = 0;
            // forward leg
            if (!moveAndMeasure(hooks, params, +params.lengths[i], dc))
            {
                err = FAULT_CAL_FAILED;
                return false;
            }
            xs[n] = (double)params.lengths[i];
            ys[n] = (double)dc;
            n++;
            // return leg (keeps travel bounded around the origin)
            if (!moveAndMeasure(hooks, params, -params.lengths[i], dc))
            {
                err = FAULT_CAL_FAILED;
                return false;
            }
            if (n < maxPts)
            {
                xs[n] = -(double)params.lengths[i];
                ys[n] = (double)dc;
                n++;
            }
        }

        Regression reg = linearRegress(xs, ys, n);
        if (!reg.ok)
        {
            err = FAULT_CAL_FAILED;
            ESP_LOGE(TAG, "SCALE: regression failed (n=%d)", n);
            return false;
        }
        if (reg.r2 < params.minR2)
        {
            err = FAULT_CAL_FAILED;
            ESP_LOGE(TAG, "SCALE: R^2=%.4f below gate %.4f — nonlinear/noisy",
                     reg.r2, params.minR2);
            return false;
        }

        double slopeMag = fabs(reg.slope);
        if (slopeMag < 1e-6)
        {
            err = FAULT_CAL_FAILED;
            ESP_LOGE(TAG, "SCALE: slope ~0");
            return false;
        }

        out.countsPerStep_q16 = (int32_t)llround(slopeMag * 65536.0);
        out.countSign = (reg.slope >= 0) ? 1 : -1;
        // Residual scatter -> the basis for verify tolerance & watchdog noise.
        // Floor at 1 count so downstream thresholds are never zero.
        double scatter = reg.residualStd;
        if (scatter < 1.0)
            scatter = 1.0;
        out.residualScatter = (uint16_t)llround(scatter);
        out.quality = (uint8_t)llround(constrain(reg.r2, 0.0, 1.0) * 100.0);

        ESP_LOGI(TAG, "SCALE: countsPerStep=%.5f (q16=%d) sign=%d R2=%.4f scatter=%u q=%u",
                 slopeMag, out.countsPerStep_q16, out.countSign, reg.r2,
                 out.residualScatter, out.quality);

        // ---- Step 4: BACKLASH ----------------------------------------------
        // Approach a point from +, reverse, and measure the counts "lost"
        // before the encoder resumes tracking the commanded motion.
        int64_t backlashAccum = 0;
        uint8_t backlashN = 0;
        for (uint8_t r = 0; r < params.backlashRepeats; r++)
        {
            int64_t dcFwd = 0, dcRev = 0;
            if (!moveAndMeasure(hooks, params, +params.backlashApproach, dcFwd))
            {
                err = FAULT_CAL_FAILED;
                return false;
            }
            if (!moveAndMeasure(hooks, params, -params.backlashApproach, dcRev))
            {
                err = FAULT_CAL_FAILED;
                return false;
            }
            // Expected counts for the reverse leg from the measured scale.
            double expectedRev = slopeMag * (double)params.backlashApproach;
            // Slack = expected minus what actually registered (magnitude).
            int64_t slack = (int64_t)llround(expectedRev - (double)llabs(dcRev));
            if (slack < 0)
                slack = 0;
            backlashAccum += slack;
            backlashN++;
        }
        out.backlashCounts = (backlashN > 0) ? (int32_t)(backlashAccum / backlashN) : 0;
        ESP_LOGI(TAG, "BACKLASH: %d counts (avg of %u)", out.backlashCounts, backlashN);

        // ---- Step 5: finalize ----------------------------------------------
        out.timestamp = millis();
        out.valid = true;
        out.derived = false;
        err = FAULT_NONE;
        return true;
    }
}

namespace AxisCalibrationStore
{
    static const char *NS = "axiscal";

    static String keyFor(int axis) { return String("cal") + String(axis); }

    bool save(int axis, const AxisCalibration &cal)
    {
        Preferences prefs;
        if (!prefs.begin(NS, false))
            return false;
        size_t written = prefs.putBytes(keyFor(axis).c_str(), &cal, sizeof(AxisCalibration));
        prefs.end();
        bool ok = (written == sizeof(AxisCalibration));
        ESP_LOGI(TAG, "save axis %d: %s (cps=%d sign=%d ms=%u)", axis, ok ? "ok" : "FAIL",
                 cal.countsPerStep_q16, cal.countSign, cal.microstepsAtCal);
        return ok;
    }

    bool load(int axis, AxisCalibration &cal)
    {
        Preferences prefs;
        if (!prefs.begin(NS, true))
            return false;
        size_t len = prefs.getBytesLength(keyFor(axis).c_str());
        bool ok = false;
        if (len == sizeof(AxisCalibration))
        {
            prefs.getBytes(keyFor(axis).c_str(), &cal, sizeof(AxisCalibration));
            ok = cal.valid;
        }
        prefs.end();
        return ok;
    }

    void clear(int axis)
    {
        Preferences prefs;
        if (!prefs.begin(NS, false))
            return;
        prefs.remove(keyFor(axis).c_str());
        prefs.end();
    }

    bool applyMicrostepRule(AxisCalibration &cal, uint16_t currentMicrosteps)
    {
        if (!cal.valid || currentMicrosteps == 0 || cal.microstepsAtCal == 0)
            return false;
        if (currentMicrosteps == cal.microstepsAtCal)
            return false;

        // Finer microsteps -> smaller steps -> fewer counts per step:
        //   cps_new = cps_old * microstepsAtCal / currentMicrosteps
        int64_t rescaled = ((int64_t)cal.countsPerStep_q16 * (int64_t)cal.microstepsAtCal) /
                           (int64_t)currentMicrosteps;
        int64_t oldBacklash = (int64_t)cal.backlashCounts;

        cal.countsPerStep_q16 = (int32_t)rescaled;
        // Backlash is a mechanical count quantity; it does not scale with
        // microstepping, so leave backlashCounts as-is.
        (void)oldBacklash;
        cal.microstepsAtCal = currentMicrosteps;
        cal.derived = true; // unverified — re-calibration advised
        ESP_LOGW(TAG, "Microsteps changed -> rescaled countsPerStep to q16=%d (derived, CAL_INVALID warning)",
                 cal.countsPerStep_q16);
        return true;
    }
}
