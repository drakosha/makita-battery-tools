/*
 * Makita Battery Reader - Serial Output Functions
 */

#ifndef MAKITA_PRINT_H
#define MAKITA_PRINT_H

#include "config.h"

void printSeparator();
void printHeader();
void printModel();
void printBatteryInfo();
void printVoltages();
void printRawData();
void printDiagnosis();
void printMenu();

#endif
