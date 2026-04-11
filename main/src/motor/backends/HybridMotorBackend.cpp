#include <PinConfig.h>
#include "HybridMotorBackend.h"
#include "esp_log.h"

static const char* TAG = "HybridMotorBE";

HybridMotorBackend::~HybridMotorBackend()
{
    for (auto& b : _backends) {
        delete b;
        b = nullptr;
    }
}

void HybridMotorBackend::setAxisBackend(int axis, IMotorBackend* backend)
{
    if (axis < 0 || axis >= MAX_AXES) return;
    delete _backends[axis];
    _backends[axis] = backend;
}

void HybridMotorBackend::setup()
{
    for (int i = 0; i < MAX_AXES; i++) {
        if (_backends[i]) {
            ESP_LOGI(TAG, "Axis %d → %s", i, _backends[i]->name());
            _backends[i]->setup();
        }
    }
}

void HybridMotorBackend::loop()
{
    for (auto* b : _backends) {
        if (b) b->loop();
    }
}

void HybridMotorBackend::startMove(int axis, MotorData* data, int reduced)
{
    if (axis < 0 || axis >= MAX_AXES || !_backends[axis]) {
        ESP_LOGW(TAG, "No backend for axis %d", axis);
        return;
    }
    _backends[axis]->startMove(axis, data, reduced);
}

void HybridMotorBackend::stopMove(int axis)
{
    if (axis < 0 || axis >= MAX_AXES || !_backends[axis]) return;
    _backends[axis]->stopMove(axis);
}

long HybridMotorBackend::getPosition(int axis) const
{
    if (axis < 0 || axis >= MAX_AXES || !_backends[axis]) return 0;
    return _backends[axis]->getPosition(axis);
}

bool HybridMotorBackend::isRunning(int axis) const
{
    if (axis < 0 || axis >= MAX_AXES || !_backends[axis]) return false;
    return _backends[axis]->isRunning(axis);
}

void HybridMotorBackend::updateData(int axis)
{
    if (axis < 0 || axis >= MAX_AXES || !_backends[axis]) return;
    _backends[axis]->updateData(axis);
}

void HybridMotorBackend::setPosition(int axis, int pos)
{
    if (axis < 0 || axis >= MAX_AXES || !_backends[axis]) return;
    _backends[axis]->setPosition(axis, pos);
}

void HybridMotorBackend::setAutoEnable(bool enable)
{
    for (auto* b : _backends) {
        if (b) b->setAutoEnable(enable);
    }
}

void HybridMotorBackend::setEnable(bool enable)
{
    for (auto* b : _backends) {
        if (b) b->setEnable(enable);
    }
}
