// T4.1: AxisCalibration — pure C (was C++ namespaces with std::function hooks;
// the hooks are plain function pointers + a void* ctx). NVS store uses the IDF
// nvs API directly (IDF 6.1: nvs_open takes the handle as an out parameter).
static const char *TAG = "AxisCal";
#include "AxisCalibration.h"
#include "uc2_hal/uc2_log.h"
#include "uc2_hal/uc2_time.h"
#include <math.h>
#include <stdlib.h> // llabs(), abs()
#include <stdio.h>  // snprintf (NVS key)
#include <nvs.h>

// ------------------------------------------------------------------ regression
// Ordinary least-squares fit of y = slope*x + intercept over n points.
// Also returns R^2 and the residual standard deviation (in y units).
typedef struct CalRegression {
    double slope, intercept, r2, residualStd;
    bool ok;
} CalRegression;

static CalRegression calLinearRegress(const double *x, const double *y, int n)
{
    CalRegression r = {0, 0, 0, 0, false};
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
    (void)syy;
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

// --------------------------------------------------------------------- the run
static AxisCalibrationLastRun s_lastRun;

const AxisCalibrationLastRun *axis_cal_last_run(void)
{
    return &s_lastRun;
}

void axis_cal_params_default(AxisCalibrationParams *p, uint16_t currentMicrosteps)
{
    p->probeSpeed = 2000;
    p->sweepSegments = 10;
    p->segmentSteps = 500;
    p->minCountsForValidity = 1000;
    p->minR2 = 0.98f;
    p->maxSlopeMismatch = 0.10f;
    p->settleMs = 60;
    p->currentMicrosteps = currentMicrosteps;
}

// Move and, on return, report how many counts moved. Returns false on abort.
static bool calMoveAndMeasure(const AxisCalibrationHooks *h, const AxisCalibrationParams *p,
                              int32_t deltaSteps, int64_t *deltaCountsOut)
{
    int64_t before = h->get_raw_count(h->ctx);
    if (!h->move_rel_blocking(h->ctx, deltaSteps, p->probeSpeed))
        return false;
    delay(p->settleMs);
    *deltaCountsOut = h->get_raw_count(h->ctx) - before;
    return true;
}

static void calRecordPoint(int32_t st, int64_t cnt)
{
    if (s_lastRun.nPoints < AXIS_CAL_MAX_POINTS)
    {
        s_lastRun.stepsAtPoint[s_lastRun.nPoints] = st;
        s_lastRun.countsAtPoint[s_lastRun.nPoints] = (int32_t)cnt;
        s_lastRun.nPoints++;
    }
}

bool axis_cal_run(const AxisCalibrationHooks *hooks, const AxisCalibrationParams *params,
                  AxisCalibration *out, AxisFault *err)
{
    *err = FAULT_NONE;
    *out = (AxisCalibration){0};
    out->microstepsAtCal = params->currentMicrosteps;
    s_lastRun = (AxisCalibrationLastRun){0}; // fresh diagnostics for this run

    if (!hooks->get_step_pos || !hooks->move_rel_blocking || !hooks->get_raw_count || !hooks->aborted)
    {
        *err = FAULT_CAL_FAILED;
        log_e("Calibration hooks not fully wired");
        return false;
    }
    if (hooks->aborted(hooks->ctx))
    {
        *err = FAULT_CAL_FAILED;
        log_e("Cannot calibrate: axis already in an abort state (endstop?)");
        return false;
    }

    uint8_t nSeg = params->sweepSegments;
    if (nSeg < 3)  nSeg = 3;
    if (nSeg > 20) nSeg = 20;             // bounds travel + LastRun capacity
    const int32_t segSteps = (params->segmentSteps != 0) ? params->segmentSteps : 500;

    log_i("Calibration start (microsteps=%u, probeSpeed=%d, %u segments x %d steps)",
          params->currentMicrosteps, params->probeSpeed, nSeg, (int)segSteps);

    // ---- Step 1: PRELOAD ------------------------------------------------
    // Take up the mechanical slack in the + direction FIRST, so the forward
    // sweep that follows contains no reversal and its slope is unbiased.
    int64_t dummy = 0;
    if (!calMoveAndMeasure(hooks, params, +segSteps, &dummy))
    {
        *err = FAULT_CAL_FAILED;
        log_e("PRELOAD: aborted");
        return false;
    }

    // ---- Step 2: FORWARD SWEEP -----------------------------------------
    double xs[AXIS_CAL_MAX_POINTS], ys[AXIS_CAL_MAX_POINTS];
    int nF = 0;
    int32_t cumSteps = 0;

    int64_t c0 = hooks->get_raw_count(hooks->ctx);
    xs[nF] = 0; ys[nF] = (double)c0; nF++;
    calRecordPoint(0, c0);

    for (uint8_t i = 0; i < nSeg; i++)
    {
        int64_t dc = 0;
        if (!calMoveAndMeasure(hooks, params, +segSteps, &dc))
        {
            *err = FAULT_CAL_FAILED;
            log_e("FORWARD: aborted at segment %u", i);
            return false;
        }
        cumSteps += segSteps;
        int64_t cnt = hooks->get_raw_count(hooks->ctx);
        xs[nF] = (double)cumSteps; ys[nF] = (double)cnt; nF++;
        calRecordPoint(cumSteps, cnt);
        log_i("FWD seg %u: steps=%+d counts=%+lld (dc=%+lld)", i, cumSteps, cnt, dc);
    }
    s_lastRun.nForward = (uint8_t)nF;

    CalRegression regF = calLinearRegress(xs, ys, nF);
    if (!regF.ok)
    {
        *err = FAULT_CAL_FAILED;
        log_e("FORWARD: regression failed (n=%d)", nF);
        return false;
    }

    // Sanity: the sweep must actually have moved the encoder.
    int64_t totalCounts = (int64_t)(ys[nF - 1] - ys[0]);
    if (llabs(totalCounts) < (int64_t)params->minCountsForValidity)
    {
        *err = FAULT_CAL_FAILED;
        log_e("FORWARD: encoder barely moved (%lld counts) - dead/unwired?",
              totalCounts);
        return false;
    }

    // ---- Step 3: REVERSE SWEEP ------------------------------------------
    // Same segments, opposite direction. The reversal happens exactly ONCE,
    // between the legs, so its backlash offsets this leg as a whole (the
    // intercept) and leaves the slope clean.
    double xr[AXIS_CAL_MAX_POINTS], yr[AXIS_CAL_MAX_POINTS];
    int nR = 0;
    for (uint8_t i = 0; i < nSeg; i++)
    {
        int64_t dc = 0;
        if (!calMoveAndMeasure(hooks, params, -segSteps, &dc))
        {
            *err = FAULT_CAL_FAILED;
            log_e("REVERSE: aborted at segment %u", i);
            return false;
        }
        cumSteps -= segSteps;
        int64_t cnt = hooks->get_raw_count(hooks->ctx);
        xr[nR] = (double)cumSteps; yr[nR] = (double)cnt; nR++;
        calRecordPoint(cumSteps, cnt);
        log_i("REV seg %u: steps=%+d counts=%+lld (dc=%+lld)", i, cumSteps, cnt, dc);
    }

    CalRegression regR = calLinearRegress(xr, yr, nR);
    if (!regR.ok)
    {
        *err = FAULT_CAL_FAILED;
        log_e("REVERSE: regression failed (n=%d)", nR);
        return false;
    }

    // Return to the starting point (undo the preload segment).
    if (!calMoveAndMeasure(hooks, params, -segSteps, &dummy))
        log_w("Could not return to start after sweep");

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
    if (worstR2 < params->minR2)
    {
        *err = FAULT_CAL_FAILED;
        log_e("SCALE: R^2=%.4f below gate %.4f - nonlinear/noisy",
              worstR2, (double)params->minR2);
        return false;
    }
    if ((regF.slope >= 0) != (regR.slope >= 0))
    {
        *err = FAULT_CAL_FAILED;
        log_e("SCALE: forward/reverse slopes disagree in sign (%.5f vs %.5f)",
              regF.slope, regR.slope);
        return false;
    }
    double magF = fabs(regF.slope), magR = fabs(regR.slope);
    double slopeMag = 0.5 * (magF + magR);
    if (slopeMag < 1e-6)
    {
        *err = FAULT_CAL_FAILED;
        log_e("SCALE: slope ~0");
        return false;
    }
    double mismatch = fabs(magF - magR) / slopeMag;
    if (mismatch > params->maxSlopeMismatch)
    {
        *err = FAULT_CAL_FAILED;
        log_e("SCALE: fwd/rev slope mismatch %.1f%% exceeds %.1f%% - "
                      "check for slip or a loose coupling",
              mismatch * 100.0, params->maxSlopeMismatch * 100.0);
        return false;
    }

    out->countsPerStep_q16 = (int32_t)llround(slopeMag * 65536.0);
    out->countSign = (regF.slope >= 0) ? 1 : -1;

    // ---- Step 5: BACKLASH from the hysteresis loop ----------------------
    // The two legs are parallel lines; their vertical separation IS the lost
    // motion. Measuring it this way needs no assumed scale, so it can't be
    // corrupted by a slope error the way the old expected-vs-actual test was.
    double loopWidth = fabs(regR.intercept - regF.intercept);
    out->backlashCounts = (int32_t)llround(loopWidth);

    double scatter = (regF.residualStd > regR.residualStd) ? regF.residualStd
                                                           : regR.residualStd;
    if (scatter < 1.0)
        scatter = 1.0;
    out->residualScatter = (uint16_t)llround(scatter);
    if (worstR2 < 0.0) worstR2 = 0.0;
    if (worstR2 > 1.0) worstR2 = 1.0;
    out->quality = (uint8_t)llround(worstR2 * 100.0);

    s_lastRun.slope = (out->countSign >= 0) ? slopeMag : -slopeMag;
    s_lastRun.intercept = regF.intercept;
    s_lastRun.r2 = worstR2;
    s_lastRun.residualStd = scatter;

    log_i("SCALE: countsPerStep=%.5f (q16=%d) sign=%d R2=%.4f scatter=%u q=%u "
          "(fwd %.5f / rev %.5f, mismatch %.2f%%)",
          slopeMag, out->countsPerStep_q16, out->countSign, worstR2,
          out->residualScatter, out->quality, magF, magR, mismatch * 100.0);
    log_i("BACKLASH: %d counts (hysteresis loop width)", out->backlashCounts);

    // ---- Step 6: finalize ----------------------------------------------
    out->timestamp = millis();
    out->valid = true;
    out->derived = false;
    *err = FAULT_NONE;
    return true;
}

// ---------------------------------------------------------------------- store
static const char *kCalNS = "axiscal";

static void calKeyFor(int axis, char *buf, size_t bufLen)
{
    snprintf(buf, bufLen, "cal%d", axis);
}

bool axis_cal_store_save(int axis, const AxisCalibration *cal)
{
    char key[16];
    calKeyFor(axis, key, sizeof key);
    nvs_handle_t h = 0;
    if (nvs_open(kCalNS, NVS_READWRITE, &h) != ESP_OK)
        return false;
    esp_err_t rc = nvs_set_blob(h, key, cal, sizeof(AxisCalibration));
    if (rc == ESP_OK)
        rc = nvs_commit(h);
    nvs_close(h);
    bool ok = (rc == ESP_OK);
    log_i("save axis %d: %s (cps=%d sign=%d ms=%u)", axis, ok ? "ok" : "FAIL",
          cal->countsPerStep_q16, cal->countSign, cal->microstepsAtCal);
    return ok;
}

bool axis_cal_store_load(int axis, AxisCalibration *cal)
{
    char key[16];
    calKeyFor(axis, key, sizeof key);
    nvs_handle_t h = 0;
    if (nvs_open(kCalNS, NVS_READONLY, &h) != ESP_OK)
        return false;
    size_t len = sizeof(AxisCalibration);
    bool ok = false;
    if (nvs_get_blob(h, key, cal, &len) == ESP_OK && len == sizeof(AxisCalibration))
        ok = cal->valid;
    nvs_close(h);
    return ok;
}

void axis_cal_store_clear(int axis)
{
    char key[16];
    calKeyFor(axis, key, sizeof key);
    nvs_handle_t h = 0;
    if (nvs_open(kCalNS, NVS_READWRITE, &h) != ESP_OK)
        return;
    nvs_erase_key(h, key);
    nvs_commit(h);
    nvs_close(h);
}

bool axis_cal_store_apply_microstep_rule(AxisCalibration *cal, uint16_t currentMicrosteps)
{
    if (!cal->valid || currentMicrosteps == 0 || cal->microstepsAtCal == 0)
        return false;
    if (currentMicrosteps == cal->microstepsAtCal)
        return false;

    // Finer microsteps -> smaller steps -> fewer counts per step:
    //   cps_new = cps_old * microstepsAtCal / currentMicrosteps
    int64_t rescaled = ((int64_t)cal->countsPerStep_q16 * (int64_t)cal->microstepsAtCal) /
                       (int64_t)currentMicrosteps;

    cal->countsPerStep_q16 = (int32_t)rescaled;
    // Backlash is a mechanical count quantity; it does not scale with
    // microstepping, so leave backlashCounts as-is.
    cal->microstepsAtCal = currentMicrosteps;
    cal->derived = true; // unverified — re-calibration advised
    log_w("Microsteps changed -> rescaled countsPerStep to q16=%d (derived, CAL_INVALID warning)",
          cal->countsPerStep_q16);
    return true;
}
