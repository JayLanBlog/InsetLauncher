#pragma once

#define LAUNCH_EXPORT_API __declspec(dllexport)
#define MARK_DEFINE()                                                 \
  extern "C" LAUNCH_EXPORT_API void  launch__replay__marker() \
  {                                                                             \
    }
