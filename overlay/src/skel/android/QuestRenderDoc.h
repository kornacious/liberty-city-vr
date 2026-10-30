#pragma once
#include "RenderDocFrameCapture.h"
namespace androidgame {
bool QuestRenderDocAvailable();
questcapture::RenderDocFrame::Result QuestRenderDocBegin(void *vkInstance,const char *prefix);
questcapture::RenderDocFrame::Result QuestRenderDocEnd(bool keep);
bool QuestRenderDocActive();
const char *QuestRenderDocCapturePath();
}
