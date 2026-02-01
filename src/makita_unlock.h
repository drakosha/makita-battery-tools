/*
 * Makita Battery Reader - Unlock and Reset Functions
 */

#ifndef MAKITA_UNLOCK_H
#define MAKITA_UNLOCK_H

#include "config.h"

// MSG operations
void saveMSG();
void compareMSG();
void cloneMSG();

// Reset operations
void resetBatteryErrors();
void unlockBattery();
void factoryResetBattery();
void resetHandshakeState();
void resetCycleCount();
void advancedResetMenu();

// Charger diagnostics
void diagnoseChargerHandshake();

#endif
