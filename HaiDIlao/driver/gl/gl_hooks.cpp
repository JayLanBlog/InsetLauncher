#include "driver/gl/table/gl_dispatch_table.h"


bool FullyImplementedFunction(const char* funcname)
{
    return
        // GL_GREMEDY_frame_terminator
        !strcmp(funcname, "glFrameTerminatorGREMEDY") ||
        // GL_GREMEDY_string_marker
        !strcmp(funcname, "glStringMarkerGREMEDY") ||
        // GL_EXT_debug_label
        !strcmp(funcname, "glLabelObjectEXT") || !strcmp(funcname, "glGetObjectLabelEXT") ||
        // GL_EXT_debug_marker
        !strcmp(funcname, "glInsertEventMarkerEXT") || !strcmp(funcname, "glPushGroupMarkerEXT") ||
        !strcmp(funcname, "glPopGroupMarkerEXT") ||
        // GL_KHR_debug (Core variants)
        !strcmp(funcname, "glDebugMessageControl") || !strcmp(funcname, "glDebugMessageInsert") ||
        !strcmp(funcname, "glDebugMessageCallback") || !strcmp(funcname, "glGetDebugMessageLog") ||
        !strcmp(funcname, "glGetPointerv") || !strcmp(funcname, "glPushDebugGroup") ||
        !strcmp(funcname, "glPopDebugGroup") || !strcmp(funcname, "glObjectLabel") ||
        !strcmp(funcname, "glGetObjectLabel") || !strcmp(funcname, "glObjectPtrLabel") ||
        !strcmp(funcname, "glGetObjectPtrLabel") ||
        // GL_KHR_debug (KHR variants)
        !strcmp(funcname, "glDebugMessageControlKHR") ||
        !strcmp(funcname, "glDebugMessageInsertKHR") ||
        !strcmp(funcname, "glDebugMessageCallbackKHR") ||
        !strcmp(funcname, "glGetDebugMessageLogKHR") || !strcmp(funcname, "glGetPointervKHR") ||
        !strcmp(funcname, "glPushDebugGroupKHR") || !strcmp(funcname, "glPopDebugGroupKHR") ||
        !strcmp(funcname, "glObjectLabelKHR") || !strcmp(funcname, "glGetObjectLabelKHR") ||
        !strcmp(funcname, "glObjectPtrLabelKHR") || !strcmp(funcname, "glGetObjectPtrLabelKHR");
}