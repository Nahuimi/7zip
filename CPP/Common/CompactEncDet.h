#ifndef ZIP7_INC_COMMON_COMPACT_ENC_DET_H
#define ZIP7_INC_COMMON_COMPACT_ENC_DET_H

#include "MyString.h"
#include "MyVector.h"

namespace NCompactEncDet {

bool DetectCodePage(const CObjectVector<AString> &samples,
    UInt32 &codePage, bool &isReliable);

}

#endif
