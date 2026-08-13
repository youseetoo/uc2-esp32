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
    static LastRun s_lastRun;
    const LastRun &lastRun() { return s_lastRun; }

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
        s_lastRun = LastRun(); // fresh diagnostics for this run

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

        uint8_t nSeg = params.sweepSegments;
        if (nSeg < 3)  nSeg = 3;
        if (nSeg > 20) nSeg = 20;             // bounds travel + LastRun capacity
        const int32_t segSteps = (params.segmentSteps != 0) ? params.segmentSteps : 500;

        log_i("Calibration start (microsteps=%u, probeSpeed=%d, %u segments x %d steps)",
              params.currentMicrosteps, params.probeSpeed, nSeg, (int)segSteps);

        // ---- Step 1: PRELOAD ------------------------------------------------
        // Take up the mechanical slack in the + direction FIRST, so the forward
        // sweep that follows contains no reversal and its slope is unbiased.
        int64_t dummy = 0;
        if (!moveAndMeasure(hooks, params, +segSteps, dummy))
        {
            err = FAULT_CAL_FAILED;
            ESP_LOGE(TAG, "PRELOAD: aborted");
            return false;
        }

        // ---- Step 2: FORWARD SWEEP -----------------------------------------
        double xs[LastRun::MAX_POINTS], ys[LastRun::MAX_POINTS];
        int nF = 0;
        int32_t cumSteps = 0;
        auto record = [&](int32_t st, int64_t cnt) {
            if (s_lastRun.nPoints < LastRun::MAX_POINTS)
            {
                s_lastRun.stepsAtPoint[s_lastRun.nPoints] = st;
                s_lastRun.countsAtPoint[s_lastRun.nPoints] = (int32_t)cnt;
                s_lastRun.nPoints++;
            }
        };

        int64_t c0 = hooks.getRawCount();
        xs[nF] = 0; ys[nF] = (double)c0; nF++;
        record(0, c0);

        for (uint8_t i = 0; i < nSeg; i++)
        {
            int64_t dc = 0;
            if (!moveAndMeasure(hooks, params, +segSteps, dc))
            {
                err = FAULT_CAL_FAILED;
                ESP_LOGE(TAG, "FORWARD: aborted at segment %u", i);
                return false;
            }
            cumSteps += segSteps;
            int64_t cnt = hooks.getRawCount();
            xs[nF] = (double)cumSteps; ys[nF] = (double)cnt; nF++;
            record(cumSteps, cnt);
            log_i("FWD seg %u: steps=%+d counts=%+lld (dc=%+lld)", i, cumSteps, cnt, dc);
        }
        s_lastRun.nForward = (uint8_t)nF;

        Regression regF = linearRegress(xs, ys, nF);
        if (!regF.ok)
        {
            err = FAULT_CAL_FAILED;
            ESP_LOGE(TAG, "FORWARD: regression failed (n=%d)", nF);
            return false;
        }

        // Sanity: the sweep must actually have moved the encoder.
        int64_t totalCounts = (int64_t)(ys[nF - 1] - ys[0]);
        if (llabs(totalCounts) < params.minCountsForValidity)
        {
            err = FAULT_CAL_FAILED;
            ESP_LOGE(TAG, "FORWARD: encoder barely moved (%lld counts) - dead/unwired?",
                     totalCounts);
            return false;
        }

        // ---- Step 3: REVERSE SWEEP ------------------------------------------
        // Same segments, opposite direction. The reversal happens exactly ONCE,
        // between the legs, so its backlash offsets this leg as a whole (the
        // intercept) and leaves the slope clean.
        double xr[LastRun::MAX_POINTS], yr[LastRun::MAX_POINTS];
        int nR = 0;
        for (uint8_t i = 0; i < nSeg; i++)
        {
            int64_t dc = 0;
            if (!moveAndMeasure(hooks, params, -segSteps, dc))
            {
                err = FAULT_CAL_FAILED;
                ESP_LOGE(TAG, "REVERSE: aborted at segment %u", i);
                return false;
            }
            cumSteps -= segSteps;
            int64_t cnt = hooks.getRawCount();
            xr[nR] = (double)cumSteps; yr[nR] = (double)cnt; nR++;
            record(cumSteps, cnt);
            log_i("REV seg %u: steps=%+d counts=%+lld (dc=%+lld)", i, cumSteps, cnt, dc);
        }

        Regression regR = linearRegress(xr, yr, nR);
        if (!regR.ok)
        {
            err = FAULT_CAL_FAILED;
            ESP_LOGE(TAG, "REVERSE: regression failed (n=%d)", nR);
            return false;
        }

        // Return to the starting point (undo the preload segment).
        if (!moveAndMeasure(hooks, params, -segSteps, dummy))
            ESP_LOGW(TAG, "Could not return to start after sweep");

        s_lastRun.slopeForward = regF.slope;
        s_lastRun.interceptForward = regF.intercept;
        s_lastRun.r2Forward = regF.r2;
        s_lastRun.residForward = regF.residualStd;
        s_lastRun.slopeReverse = regR.slope;
        s_lastRun.interceptReverse = regR.intercept;
        s_lastRun.r2Reverse = regR.r2;
        s_lastRun.residReverse = regR.residualStd;

        log_i("FWD fit: slope=%.5f intercept=%.2f R2=%.4f resid=%.2f",
              regF.slope, regF.intercept, regF.r2, regF.residualStd);
        log_i("REV fit: slope=%.5f intercept=%.2f R2=%.4f resid=%.2f",
              regR.slope, regR.intercept, regR.r2, regR.residualStd);

        // ---- Step 4: VALIDATE + COMBINE -------------------------------------
        double worstR2 = (regF.r2 < regR.r2) ? regF.r2 : regR.r2;
        if (worstR2 < params.minR2)
        {
            err = FAULT_CAL_FAILED;
            ESP_LOGE(TAG, "SCALE: R^2=%.4f below gate %.4f - nonlinear/noisy",
                     worstR2, params.minR2);
            return false;
        }
        if ((regF.slope >= 0) != (regR.slope >= 0))
        {
            err = FAULT_CAL_FAILED;
            ESP_LOGE(TAG, "SCALE: forward/reverse slopes disagree in sign (%.5f vs %.5f)",
                     regF.slope, regR.slope);
            return false;
        }
        double magF = fabs(regF.slope), magR = fabs(regR.slope);
        double slopeMag = 0.5 * (magF + magR);
        if (slopeMag < 1e-6)
        {
            err = FAULT_CAL_FAILED;
            ESP_LOGE(TAG, "SCALE: slope ~0");
            return false;
        }
        double mismatch = fabs(magF - magR) / slopeMag;
        if (mismatch > params.maxSlopeMismatch)
        {
            err = FAULT_CAL_FAILED;
            ESP_LOGE(TAG, "SCALE: fwd/rev slope mismatch %.1f%% exceeds %.1f%% - "
                          "check for slip or a loose coupling",
                     mismatch * 100.0, params.maxSlopeMismatch * 100.0);
            return false;
        }

        out.countsPerStep_q16 = (int32_t)llround(slopeMag * 65536.0);
        out.countSign = (regF.slope >= 0) ? 1 : -1;

        // ---- Step 5: BACKLASH from the hysteresis loop ----------------------
        // The two legs are parallel lines; their vertical separation IS the lost
        // motion. Measuring it this way needs no assumed scale, so it can't be
        // corrupted by a slope error the way the old expected-vs-actual test was.
        double loopWidth = fabs(regR.intercept - regF.intercept);
        out.backlashCounts = (int32_t)llround(loopWidth);

        double scatter = (regF.residualStd > regR.residualStd) ? regF.residualStd
                                                               : regR.residualStd;
        if (scatter < 1.0)
            scatter = 1.0;
        out.residualScatter = (uint16_t)llround(scatter);
        out.quality = (uint8_t)llround(constrain(worstR2, 0.0, 1.0) * 100.0);

        s_lastRun.slope = (out.countSign >= 0) ? slopeMag : -slopeMag;
        s_lastRun.intercept = regF.intercept;
        s_lastRun.r2 = worstR2;
        s_lastRun.residualStd = scatter;

        log_i("SCALE: countsPerStep=%.5f (q16=%d) sign=%d R2=%.4f scatter=%u q=%u "
              "(fwd %.5f / rev %.5f, mismatch %.2f%%)",
              slopeMag, out.countsPerStep_q16, out.countSign, worstR2,
              out.residualScatter, out.quality, magF, magR, mismatch * 100.0);
        log_i("BACKLASH: %d counts (hysteresis loop width)", out.backlashCounts);

        // ---- Step 6: finalize ----------------------------------------------
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
        log_i("save axis %d: %s (cps=%d sign=%d ms=%u)", axis, ok ? "ok" : "FAIL",
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
