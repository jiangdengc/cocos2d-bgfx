#pragma once
#include "base/ccConfig.h"
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WINRT || CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID || CC_TARGET_PLATFORM == CC_PLATFORM_IOS || CC_TARGET_PLATFORM == CC_PLATFORM_MAC || CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)

#include "cocos/scripting/js-bindings/jswrapper/SeApi.h"

bool register_all_cocos2dx_audioengine(se::Object* obj);

extern se::Object* __jsb_cocos2d_AudioEngine_proto;
extern se::Class* __jsb_cocos2d_AudioEngine_class;

bool register_all_cocos2dx_audioengine(se::Object* obj);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_lazyInit);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_setCurrentTime);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_getVolume);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_uncache);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_resumeAll);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_stopAll);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_pause);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_end);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_getMaxAudioInstance);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_isEnabled);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_getCurrentTime);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_setMaxAudioInstance);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_isLoop);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_pauseAll);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_uncacheAll);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_setVolume);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_preload);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_setEnabled);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_play2d);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_getState);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_resume);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_stop);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_getDuration);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_setLoop);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_setFinishCallback);
SE_DECLARE_FUNC(js_cocos2dx_audioengine_AudioEngine_getPlayingAudioCount);

#endif //#if (CC_TARGET_PLATFORM == CC_PLATFORM_WINRT || CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID || CC_TARGET_PLATFORM == CC_PLATFORM_IOS || CC_TARGET_PLATFORM == CC_PLATFORM_MAC || CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
