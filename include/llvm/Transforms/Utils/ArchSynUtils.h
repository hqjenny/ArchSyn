#ifndef ARCHSYN_UTIL_H
#define ARCHSYN_UTIL_H
#include "llvm/ADT/Statistic.h"
#include "llvm/Function.h"
#include "llvm/DerivedTypes.h"
#//define TRANSFORMEDATTR "dpp-transformed"
//#define GENERATEDATTR "dpp-generated"
//#define CHANNELATTR "dpp-channel"
//#define CHANNELWR "dpp-channel-wr"
//#define CHANNELRD "dpp-channel-rd"
//#define NORMALARGATTR "dpp-normalArg"
#define HLSDIRVARNAME "common_anc_dir"
#define BURST_INTSIZE 32
using namespace llvm;

bool cmpChannelAttr(AttrListPtr  as, int argSeqNum, Attributes::AttrVal channelStr );
bool isArgChannel(Argument* curFuncArg);

#endif
