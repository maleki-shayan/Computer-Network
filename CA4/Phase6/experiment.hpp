#ifndef EXPERIMENT_HPP
#define EXPERIMENT_HPP

#include <string>

using namespace std;

const string TEST_NAME = "fast_retransmit_on";
const int LOSS_PERCENT = 10;
const int ACK_DELAY_MS = 50;
const bool ENABLE_FAST_RETRANSMIT = true;
const bool ENABLE_FAST_RECOVERY = true;

const string PHASE6_DIRECTORY = "results/phase6/" + TEST_NAME;

#endif
