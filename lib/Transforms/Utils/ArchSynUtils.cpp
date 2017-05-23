#include "llvm/Transforms/Utils/ArchSynUtils.h"
bool cmpChannelAttr(AttrListPtr as, int argSeqNum, Attributes::AttrVal channelStr)
{
    //std::string argAttr = as.getAsString(argSeqNum+1);
    Attributes attr= as.getParamAttributes(argSeqNum + 1);
    //std::string argAttr = as.getParamAttributes(argSeqNum+1).getAsString();
    //std::string channelAttrStr = channelStr.getAsString();
    //std::string channelAttrStr = "\"";
    //channelAttrStr +=channelStr;
    //channelAttrStr +="\"";
    //return argAttr == channelAttrStr;

    return attr.hasAttribute(channelStr);
}

bool isArgChannel(Argument* curFuncArg)
{
    Function* func = curFuncArg->getParent();
    bool isWrChannel = cmpChannelAttr(func->getAttributes(), curFuncArg->getArgNo(), Attributes::CHANNELWR);
    bool isRdChannel = cmpChannelAttr(func->getAttributes(), curFuncArg->getArgNo(), Attributes::CHANNELRD);

    return isWrChannel || isRdChannel;
}
