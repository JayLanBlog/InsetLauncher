#pragma once
#include "common/common.h"


#include "official/glcorearb.h"
#include "official/glext.h"
#include "official/gl32.h"
#include "official/glesext.h"
#include "official/wglext.h"
#include "common/thread.h"


template <typename T>
T CheckConstParam(T t);


extern Threading::CriticalSection glLock;