#ifdef __ANDROID__
#include "NativeAudioOutput.h"
#include <aaudio/AAudio.h>
namespace NativeAudioOutput {
namespace { AAudioStream *stream; Callback callback; void *user; }
void Close() {
    if(stream) { AAudioStream_requestStop(stream); AAudioStream_close(stream); stream=nullptr; }
}
bool Open(Callback render, void *context) {
    Close(); callback=render; user=context;
    AAudioStreamBuilder *builder=nullptr;
    if(AAudio_createStreamBuilder(&builder)!=AAUDIO_OK) return false;
    AAudioStreamBuilder_setDirection(builder,AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setFormat(builder,AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder,2);
    AAudioStreamBuilder_setSampleRate(builder,48000);
    AAudioStreamBuilder_setPerformanceMode(builder,AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(builder,AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setDataCallback(builder,[](AAudioStream*,void*,void *data,int32_t frames) -> aaudio_data_callback_result_t {
        callback(user,data,frames); return AAUDIO_CALLBACK_RESULT_CONTINUE;
    },nullptr);
    auto result=AAudioStreamBuilder_openStream(builder,&stream);
    AAudioStreamBuilder_delete(builder);
    if(result!=AAUDIO_OK || !stream || AAudioStream_getSampleRate(stream)!=48000) { Close(); return false; }
    if(AAudioStream_requestStart(stream)!=AAUDIO_OK) { Close(); return false; }
    return true;
}
}
#endif
