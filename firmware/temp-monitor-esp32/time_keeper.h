#pragma once

#include <stdint.h>

void timeKeeperInit();
void timeKeeperPersist();
void timeKeeperTick();
bool timeKeeperIsValid();
