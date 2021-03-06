/****************************************************************************
Copyright (c) 2008-2010 Ricardo Quesada
Copyright (c) 2010-2013 cocos2d-x.org
Copyright (c) 2011      Zynga Inc.
Copyright (c) 2013-2016 Chukong Technologies Inc.

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

// cocos2d includes
#include "ccHeader.h"
#include "base/CCDirector.h"

#include "2d/CCSpriteFrameCache.h"
#include "platform/CCFileUtils.h"

#include "2d/CCActionManager.h"
#include "2d/CCFontFNT.h"
#include "2d/CCFontAtlasCache.h"
#include "2d/CCAnimationCache.h"
#include "2d/CCTransition.h"
#include "2d/CCFontFreeType.h"
#include "2d/CCLabelAtlas.h"
#include "renderer/CCTextureCache.h"
#include "base/CCUserDefault.h"
#include "base/ccFPSImages.h"
#include "base/CCScheduler.h"
#include "base/CCEventDispatcher.h"
#include "base/CCEventCustom.h"
#include "base/CCConsole.h"
#include "base/CCAutoreleasePool.h"
#include "base/CCConfiguration.h"
#include "base/CCAsyncTaskPool.h"
#include "platform/CCApplication.h"
#include "spine/SkeletonBatch.h"
#include "renderer/Renderer.h"

#include "base/Camera.h"
#include "base/View.h"
#include "base/Async.h"

#if CC_ENABLE_SCRIPT_BINDING
#include "base/CCScriptSupport.h"
#endif

/**
 Position of the FPS

 Default: 0,0 (bottom-left corner)
 */
#ifndef CC_DIRECTOR_STATS_POSITION
#define CC_DIRECTOR_STATS_POSITION SharedDirector.getVisibleOrigin()
#endif // CC_DIRECTOR_STATS_POSITION

using namespace std;

NS_CC_BEGIN
// FIXME: it should be a Director ivar. Move it there once support for multiple directors is added

// singleton stuff

#define kDefaultFPS        60  // 60 frames per second
extern const char* cocos2dVersion(void);

const char *Director::EVENT_PROJECTION_CHANGED = "director_projection_changed";
const char *Director::EVENT_AFTER_DRAW = "director_after_draw";
const char *Director::EVENT_AFTER_VISIT = "director_after_visit";
const char *Director::EVENT_BEFORE_UPDATE = "director_before_update";
const char *Director::EVENT_AFTER_UPDATE = "director_after_update";
const char *Director::EVENT_RESET = "director_reset";


Director::Director()
: _isStatusLabelUpdated(true)
, _invalid(true)
, _isCullingEnabled(true)
, camera_(Camera2D::create("Default"_slice))
, clearColor_(0xff1a1a1a)
, _displayStats(false)
{
    init();
}

bool Director::init(void)
{
    setDefaultValues();

    // scenes
    _runningScene = nullptr;
    _nextScene = nullptr;

    _notificationNode = nullptr;

    _scenesStack.reserve(15);

    // FPS
    _accumDt = 0.0f;
    _frameRate = 0.0f;
    _totalFrames = 0;
    _lastUpdate = std::chrono::steady_clock::now();
    _secondsPerFrame = 1.0f;

    // paused ?
    _paused = false;
    _invalid = false;
    
    // purge ?
    _purgeDirectorInNextLoop = false;
    
    // restart ?
    _restartDirectorInNextLoop = false;

    _winSizeInPoints = Size::ZERO;

    _openGLView = nullptr;
    
    _contentScaleFactor = 1.0f;

    _console = new (std::nothrow) Console;

    // scheduler
    _scheduler = new (std::nothrow) Scheduler();
    // action manager
    _actionManager = new (std::nothrow) ActionManager();
    _scheduler->scheduleUpdate(_actionManager.get(), Scheduler::PRIORITY_SYSTEM, false);

    _eventDispatcher = new (std::nothrow) EventDispatcher();
    _eventAfterDraw = new (std::nothrow) EventCustom(EVENT_AFTER_DRAW);
    _eventAfterDraw->setUserData(this);
    _eventAfterVisit = new (std::nothrow) EventCustom(EVENT_AFTER_VISIT);
    _eventAfterVisit->setUserData(this);
    _eventBeforeUpdate = new (std::nothrow) EventCustom(EVENT_BEFORE_UPDATE);
    _eventBeforeUpdate->setUserData(this);
    _eventAfterUpdate = new (std::nothrow) EventCustom(EVENT_AFTER_UPDATE);
    _eventAfterUpdate->setUserData(this);
    _eventProjectionChanged = new (std::nothrow) EventCustom(EVENT_PROJECTION_CHANGED);
    _eventProjectionChanged->setUserData(this);
    _eventResetDirector = new (std::nothrow) EventCustom(EVENT_RESET);
    //init TextureCache
    initTextureCache();

    //_renderer = new (std::nothrow) Renderer;

    camera_->Updated = std::bind(&Director::markDirty, this);

    return true;
}

Director::~Director(void)
{
    CCLOGINFO("deallocing Director: %p", this);

    CC_SAFE_RELEASE(_runningScene);
    CC_SAFE_RELEASE(_notificationNode);

    delete _console;

    Configuration::destroyInstance();
}

void Director::setCamera(Camera* var)
{
    if (camera_)
    {

    }
    camera_ = var ? var : Camera2D::create("Default"_slice);
    camera_->Updated = std::bind(&Director::markDirty, this);
}

Camera* Director::getCamera() const
{
    return camera_;
}

void Director::setClearColor(Color4B var)
{
    clearColor_ = var;
}

Color4B Director::getClearColor() const
{
    return clearColor_;
}

void Director::markDirty()
{

}

const Mat4& Director::getViewProjection() const
{
    return *viewProjs_.top();
}

void Director::setDefaultValues(void)
{
    Configuration *conf = Configuration::getInstance();

    // default FPS
    double fps = conf->getValue("cocos2d.x.fps", Value(kDefaultFPS)).asDouble();
    _oldAnimationInterval = _animationInterval = 1.0 / fps;

    // Display FPS
    _displayStats = conf->getValue("cocos2d.x.display_fps", Value(false)).asBool();

    // GL projection
    std::string projection = conf->getValue("cocos2d.x.gl.projection", Value("3d")).asString();
    if (projection == "3d")
        _projection = Projection::_3D;
    else if (projection == "2d")
        _projection = Projection::_2D;
    else if (projection == "custom")
        _projection = Projection::CUSTOM;
    else
        CCASSERT(false, "Invalid projection value");

    // Default pixel format for PNG images with alpha
    std::string pixel_format = conf->getValue("cocos2d.x.texture.pixel_format_for_png", Value("rgba8888")).asString();
    if (pixel_format == "rgba8888")
        Texture2D::setDefaultAlphaPixelFormat(bgfx::TextureFormat::RGBA8);
    else if(pixel_format == "rgba4444")
        Texture2D::setDefaultAlphaPixelFormat(bgfx::TextureFormat::RGBA4);
    else if(pixel_format == "rgba5551")
        Texture2D::setDefaultAlphaPixelFormat(bgfx::TextureFormat::RGB5A1);

    // PVR v2 has alpha premultiplied ?
    bool pvr_alpha_premultipled = conf->getValue("cocos2d.x.texture.pvrv2_has_alpha_premultiplied", Value(false)).asBool();
    Image::setPVRImagesHavePremultipliedAlpha(pvr_alpha_premultipled);
}

void Director::setGLDefaultValues()
{
    // This method SHOULD be called only after openGLView_ was initialized
    CCASSERT(_openGLView, "opengl view should not be null");

    setAlphaBlending(true);
    setDepthTest(false);
    setProjection(_projection);
}

// Draw the Scene
void Director::drawScene()
{
    if (!_paused)
    {
        _eventDispatcher->dispatchEvent(_eventBeforeUpdate);
        _scheduler->update(getDeltaTime());
        _eventDispatcher->dispatchEvent(_eventAfterUpdate);
    }

    if (_nextScene)
    {
        setNextScene();
    }

    Mat4 viewProj;
    bx::mtxMul(viewProj, getCamera()->getView(), SharedView.getProjection());

    Size viewSize = _openGLView->getViewPortRect().size;
    Mat4 ortho;
    bx::mtxOrtho(ortho, 0, viewSize.width, 0, viewSize.height, -1000.0f, 1000.0f, 0, bgfx::getCaps()->homogeneousDepth);

    sandwichViewProjection(viewProj, [&]()
    {
        SharedView.sandwichName("Main"_slice, [&]()
        {
            uint8_t viewId = SharedView.getId();
            bgfx::setViewClear(viewId,
                BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
                clearColor_.toRGBA());
            bgfx::setViewTransform(viewId, nullptr, getViewProjection());

            if (_runningScene)
            {
                _runningScene->render(SharedRendererManager.getCurrent(), Mat4::IDENTITY);
                SharedRendererManager.flush();
            }

        });

        if (_displayStats)
        {
            SharedView.sandwichName("UI"_slice, [&]()
            {
                showStats();
            });
        }

        SharedView.clear();
    });

    _eventDispatcher->dispatchEvent(_eventAfterDraw);

    _totalFrames++;
}

float Director::getDeltaTime() const
{
    return std::min(SharedApplication.getDeltaTime(), 1.0 / SharedApplication.getMinFPS());
}

void Director::setOpenGLView(GLView *openGLView)
{
    CCASSERT(openGLView, "opengl view should not be null");

    if (_openGLView != openGLView)
    {
        // Configuration. Gather GPU info
        //Configuration *conf = Configuration::getInstance();
        //conf->gatherGPUInfo();
        //CCLOG("%s\n",conf->getInfo().c_str());

        if(_openGLView)
            _openGLView->release();
        _openGLView = openGLView;
        _openGLView->retain();

        // set size
        _winSizeInPoints = _openGLView->getDesignResolutionSize();

        _isStatusLabelUpdated = true;

        if (_openGLView)
        {
            //setGLDefaultValues();
        }

        //_renderer->initGLView();

        //CHECK_GL_ERROR_DEBUG();

        if (_eventDispatcher)
        {
            _eventDispatcher->setEnabled(true);
        }
    }
}

TextureCache* Director::getTextureCache() const
{
    return _textureCache;
}

void Director::initTextureCache()
{
    _textureCache = &SharedTextureCache;
}

void Director::destroyTextureCache()
{

}

void Director::setViewport()
{
    if (_openGLView)
    {
        _openGLView->setViewPortInPoints(0, 0, _winSizeInPoints.width, _winSizeInPoints.height);
    }
}

void Director::setNextDeltaTimeZero(bool nextDeltaTimeZero)
{
    _nextDeltaTimeZero = nextDeltaTimeZero;
}

void Director::pushViewProjection(const Mat4& viewProj)
{
    Mat4* mat = new Mat4(viewProj);
    viewProjs_.push(MakeOwn(mat));
}

void Director::popViewProjection()
{
    viewProjs_.pop();
}

void Director::setProjection(Projection projection)
{
    Size size = _winSizeInPoints;

    if (size.width == 0 || size.height == 0)
    {
        CCLOGERROR("cocos2d: warning, Director::setProjection() failed because size is 0");
        return;
    }

    SharedView.reset();
    Camera2D* cam = static_cast<Camera2D*>(getCamera());
    cam->setPosition(Vec2(size.width / 2.0f - _openGLView->getViewPortRectFixed().origin.x / (_openGLView->getScaleX()),
        size.height / 2.0f /*- _openGLView->getViewPortRectFixed().origin.y / (_openGLView->getScaleY())*/));

    //update view
    cam->getView();

    _projection = projection;
    _eventDispatcher->dispatchEvent(_eventProjectionChanged);
    return;

    Mat4 viewProj;
    bx::mtxMul(viewProj, cam->getView(), SharedView.getProjection());

    switch (projection)
    {
    case Projection::_2D:
    {
        Mat4 orthoMatrix;
        Mat4::createOrthographicOffCenter(0, size.width, 0, size.height, -1024, 1024, &orthoMatrix);
        /*loadMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_VIEWPROJECTION, orthoMatrix);
        loadIdentityMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELWORLD);*/
        break;
    }

    case Projection::_3D:
    {
        float zeye = this->getZEye();

        Mat4 matrixPerspective, matrixLookup;

        // issue #1334
        Mat4::createPerspective(60, (GLfloat)size.width / size.height, 10, zeye + size.height / 2, &matrixPerspective);

        Vec3 eye(size.width / 2, size.height / 2, zeye), center(size.width / 2, size.height / 2, 0.0f), up(0.0f, 1.0f, 0.0f);
        Mat4::createLookAt(eye, center, up, &matrixLookup);
        Mat4 proj3d = matrixPerspective * matrixLookup;

        /*loadMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_VIEWPROJECTION, proj3d);
        loadIdentityMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELWORLD);*/
        break;
    }

    case Projection::CUSTOM:
        // Projection Delegate is no longer needed
        // since the event "PROJECTION CHANGED" is emitted
        break;

    default:
        CCLOG("cocos2d: Director: unrecognized projection");
        break;
    }

    return;

    setViewport();


    _projection = projection;
    /*GL::setProjectionMatrixDirty();*/

    _eventDispatcher->dispatchEvent(_eventProjectionChanged);
}

void Director::purgeCachedData(void)
{
    FontFNT::purgeCachedData();
    FontAtlasCache::purgeCachedData();

    if (getOpenGLView())
    {
        SpriteFrameCache::getInstance()->removeUnusedSpriteFrames();
        _textureCache->removeUnusedTextures();

        // Note: some tests such as ActionsTest are leaking refcounted textures
        // There should be no test textures left in the cache
        log("%s\n", _textureCache->getCachedTextureInfo().c_str());
    }
    FileUtils::getInstance()->purgeCachedEntries();
}

float Director::getZEye(void) const
{
    return (_winSizeInPoints.height / 1.154700538379252f);//(2 * tanf(M_PI/6))
}

void Director::setAlphaBlending(bool on)
{
    //if (on)
    //{
    //    GL::blendFunc(CC_BLEND_SRC, CC_BLEND_DST);
    //}
    //else
    //{
    //    GL::blendFunc(GL_ONE, GL_ZERO);
    //}

    //CHECK_GL_ERROR_DEBUG();
}

void Director::setDepthTest(bool on)
{
    //_renderer->setDepthTest(on);
}

void Director::setClearColor(const Color4F& clearColor)
{
    clearColor_ = clearColor;
}

static void GLToClipTransform(Mat4 *transformOut)
{
    if(nullptr == transformOut) return;

    //auto d = SharedDirector; //this will cause a compile error, very strange
    auto director = &SharedDirector;

    Mat4 viewProj;
    bx::mtxMul(viewProj, director->getCamera()->getView(), SharedView.getProjection());
    *transformOut = viewProj;
}

//used for touch hit, cocos2d-js:_touchStartHandler
Vec2 Director::convertToGL(const Vec2& uiPoint)
{
    Mat4 transform;
    GLToClipTransform(&transform);

    Mat4 transformInv = transform.getInversed();

    // Calculate z=0 using -> transform*[0, 0, 0, 1]/w
    float zClip = transform.m[14]/transform.m[15];

    Size glSize = _openGLView->getDesignResolutionSize();
    Vec4 clipCoord(2.0f*uiPoint.x/glSize.width - 1.0f, 1.0f - 2.0f*uiPoint.y/glSize.height, zClip, 1);

    Vec4 glCoord;
    //transformInv.transformPoint(clipCoord, &glCoord);
    transformInv.transformVector(clipCoord, &glCoord);
    float factor = 1.0f / glCoord.w;
    return Vec2(glCoord.x * factor, glCoord.y * factor);
}

Vec2 Director::convertToUI(const Vec2& glPoint)
{
    Mat4 transform;
    GLToClipTransform(&transform);

    Vec4 clipCoord;
    // Need to calculate the zero depth from the transform.
    Vec4 glCoord(glPoint.x, glPoint.y, 0.0, 1);
    transform.transformVector(glCoord, &clipCoord);

    /*
    BUG-FIX #5506

    a = (Vx, Vy, Vz, 1)
    b = (a×M)T
    Out = 1 ⁄ bw(bx, by, bz)
    */

    clipCoord.x = clipCoord.x / clipCoord.w;
    clipCoord.y = clipCoord.y / clipCoord.w;
    clipCoord.z = clipCoord.z / clipCoord.w;

    Size glSize = _openGLView->getDesignResolutionSize();
    float factor = 1.0f / glCoord.w;
    return Vec2(glSize.width * (clipCoord.x * 0.5f + 0.5f) * factor, glSize.height * (-clipCoord.y * 0.5f + 0.5f) * factor);
}

const Size& Director::getWinSize(void) const
{
    return _winSizeInPoints;
}

Size Director::getWinSizeInPixels() const
{
    return Size(_winSizeInPoints.width * _contentScaleFactor, _winSizeInPoints.height * _contentScaleFactor);
}

Size Director::getVisibleSize() const
{
    if (_openGLView)
    {
        return _openGLView->getVisibleSize();
    }
    else
    {
        return Size::ZERO;
    }
}

Vec2 Director::getVisibleOrigin() const
{
    if (_openGLView)
    {
        return _openGLView->getVisibleOrigin();
    }
    else
    {
        return Vec2::ZERO;
    }
}

Rect Director::getSafeAreaRect() const
{
    if (_openGLView)
    {
        return _openGLView->getSafeAreaRect();
    }
    else
    {
        return Rect::ZERO;
    }
}

// scene management

void Director::runWithScene(Scene *scene)
{
    CCASSERT(scene != nullptr, "This command can only be used to start the Director. There is already a scene present.");
    CCASSERT(_runningScene == nullptr, "_runningScene should be null");

    pushScene(scene);
    startAnimation();
}

void Director::replaceScene(Scene *scene)
{
    //CCASSERT(_runningScene, "Use runWithScene: instead to start the director");
    CCASSERT(scene != nullptr, "the scene should not be null");

    if (_runningScene == nullptr) {
        runWithScene(scene);
        return;
    }

    if (scene == _nextScene)
        return;

    if (_nextScene)
    {
        if (_nextScene->isRunning())
        {
            _nextScene->onExit();
        }
        _nextScene->cleanup();

#if CC_ENABLE_GC_FOR_NATIVE_OBJECTS
        auto sEngine = ScriptEngineManager::getInstance()->getScriptEngine();
        if (sEngine)
        {
            sEngine->releaseScriptObject(this, _nextScene);
        }
#endif // CC_ENABLE_GC_FOR_NATIVE_OBJECTS
        _nextScene = nullptr;
    }

    ssize_t index = _scenesStack.size() - 1;

    _sendCleanupToScene = true;
#if CC_ENABLE_GC_FOR_NATIVE_OBJECTS
    auto sEngine = ScriptEngineManager::getInstance()->getScriptEngine();
    if (sEngine)
    {
        sEngine->retainScriptObject(this, scene);
    }
#endif // CC_ENABLE_GC_FOR_NATIVE_OBJECTS
    _scenesStack.replace(index, scene);

    _nextScene = scene;
}

void Director::pushScene(Scene *scene)
{
    CCASSERT(scene, "the scene should not null");

    _sendCleanupToScene = false;

#if CC_ENABLE_GC_FOR_NATIVE_OBJECTS
    auto sEngine = ScriptEngineManager::getInstance()->getScriptEngine();
    if (sEngine)
    {
        sEngine->retainScriptObject(this, scene);
    }
#endif // CC_ENABLE_GC_FOR_NATIVE_OBJECTS
    _scenesStack.pushBack(scene);
    _nextScene = scene;
}

void Director::popScene(void)
{
    CCASSERT(_runningScene != nullptr, "running scene should not null");
    
#if CC_ENABLE_GC_FOR_NATIVE_OBJECTS
    auto sEngine = ScriptEngineManager::getInstance()->getScriptEngine();
    if (sEngine)
    {
        sEngine->releaseScriptObject(this, _scenesStack.back());
    }
#endif // CC_ENABLE_GC_FOR_NATIVE_OBJECTS
    _scenesStack.popBack();
    ssize_t c = _scenesStack.size();

    if (c == 0)
    {
        end();
    }
    else
    {
        _sendCleanupToScene = true;
        _nextScene = _scenesStack.at(c - 1);
    }
}

void Director::popToRootScene(void)
{
    popToSceneStackLevel(1);
}

void Director::popToSceneStackLevel(int level)
{
    CCASSERT(_runningScene != nullptr, "A running Scene is needed");
    ssize_t c = _scenesStack.size();

    // level 0? -> end
    if (level == 0)
    {
        end();
        return;
    }

    // current level or lower -> nothing
    if (level >= c)
        return;

    auto firstOnStackScene = _scenesStack.back();
    if (firstOnStackScene == _runningScene)
    {
#if CC_ENABLE_GC_FOR_NATIVE_OBJECTS
        auto sEngine = ScriptEngineManager::getInstance()->getScriptEngine();
        if (sEngine)
        {
            sEngine->releaseScriptObject(this, _scenesStack.back());
        }
#endif // CC_ENABLE_GC_FOR_NATIVE_OBJECTS
        _scenesStack.popBack();
        --c;
    }

    // pop stack until reaching desired level
    while (c > level)
    {
        auto current = _scenesStack.back();

        if (current->isRunning())
        {
            current->onExit();
        }

        current->cleanup();
#if CC_ENABLE_GC_FOR_NATIVE_OBJECTS
        auto sEngine = ScriptEngineManager::getInstance()->getScriptEngine();
        if (sEngine)
        {
            sEngine->releaseScriptObject(this, _scenesStack.back());
        }
#endif // CC_ENABLE_GC_FOR_NATIVE_OBJECTS
        _scenesStack.popBack();
        --c;
    }

    _nextScene = _scenesStack.back();

    // cleanup running scene
    _sendCleanupToScene = true;
}

void Director::end()
{
    _purgeDirectorInNextLoop = true;
}

void Director::restart()
{
    _restartDirectorInNextLoop = true;
}

void Director::reset()
{
#if CC_ENABLE_GC_FOR_NATIVE_OBJECTS
    auto sEngine = ScriptEngineManager::getInstance()->getScriptEngine();
#endif // CC_ENABLE_GC_FOR_NATIVE_OBJECTS
    
    if (_runningScene)
    {
#if CC_ENABLE_GC_FOR_NATIVE_OBJECTS
        if (sEngine)
        {
            sEngine->releaseScriptObject(this, _runningScene);
        }
#endif // CC_ENABLE_GC_FOR_NATIVE_OBJECTS
        _runningScene->onExit();
        _runningScene->cleanup();
        _runningScene->release();
    }

    _runningScene = nullptr;
    _nextScene = nullptr;

    _eventDispatcher->dispatchEvent(_eventResetDirector);

    // cleanup scheduler
    getScheduler()->unscheduleAll();
    getScheduler()->removeAllFunctionsToBePerformedInCocosThread();

    // Remove all events
    if (_eventDispatcher)
    {
        _eventDispatcher->removeAllEventListeners();
    }

    // remove all objects, but don't release it.
    // runWithScene might be executed after 'end'.
#if CC_ENABLE_GC_FOR_NATIVE_OBJECTS
    if (sEngine)
    {
        for (const auto &scene : _scenesStack)
        {
            if (scene)
                sEngine->releaseScriptObject(this, scene);
        }
    }
#endif // CC_ENABLE_GC_FOR_NATIVE_OBJECTS
    _scenesStack.clear();

    stopAnimation();

    CC_SAFE_RELEASE_NULL(_notificationNode);

    // purge bitmap cache
    FontFNT::purgeCachedData();
    FontAtlasCache::purgeCachedData();
    FontFreeType::shutdownFreeType();

    // purge all managed caches

#if defined(__GNUC__) && ((__GNUC__ >= 4) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1)))
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif _MSC_VER >= 1400 //vs 2005 or higher
#pragma warning (push)
#pragma warning (disable: 4996)
#endif
//it will crash clang static analyzer so hide it if __clang_analyzer__ defined
#ifndef __clang_analyzer__

#endif
#if defined(__GNUC__) && ((__GNUC__ >= 4) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1)))
#pragma GCC diagnostic warning "-Wdeprecated-declarations"
#elif _MSC_VER >= 1400 //vs 2005 or higher
#pragma warning (pop)
#endif
    AnimationCache::destroyInstance();
    SpriteFrameCache::destroyInstance();
    FileUtils::destroyInstance();
    AsyncTaskPool::destroyInstance();
    spine::SkeletonBatch::destroyInstance();
    SharedAsyncThread.stop();
    
    // cocos2d-x specific data structures
    UserDefault::destroyInstance();

    SharedTextureCache.removeAllTextures();
}

void Director::purgeDirector()
{
    reset();

    //CHECK_GL_ERROR_DEBUG();

    // OpenGL view
    if (_openGLView)
    {
        _openGLView->end();
        _openGLView = nullptr;
    }

    // delete Director
    release();
}

void Director::restartDirector()
{
    reset();

    // Texture cache need to be reinitialized
    initTextureCache();

    // Reschedule for action manager
    getScheduler()->scheduleUpdate(getActionManager(), Scheduler::PRIORITY_SYSTEM, false);

    // release the objects
    PoolManager::getInstance()->getCurrentPool()->clear();

    // Restart animation
    startAnimation();
    
    // Real restart in script level
#if CC_ENABLE_SCRIPT_BINDING
    ScriptEvent scriptEvent(kRestartGame, nullptr);
    ScriptEngineManager::getInstance()->getScriptEngine()->sendEvent(&scriptEvent);
#endif
}

void Director::setNextScene()
{
    bool runningIsTransition = CocosCast<TransitionScene>(_runningScene) != nullptr;
    bool newIsTransition = CocosCast<TransitionScene>(_nextScene) != nullptr;

    // If it is not a transition, call onExit/cleanup
    if (! newIsTransition)
    {
        if (_runningScene)
        {
            _runningScene->onExitTransitionDidStart();
            _runningScene->onExit();
        }

        // issue #709. the root node (scene) should receive the cleanup message too
        // otherwise it might be leaked.
        if (_sendCleanupToScene && _runningScene)
        {
            _runningScene->cleanup();
        }
    }

    if (_runningScene)
    {
#if CC_ENABLE_GC_FOR_NATIVE_OBJECTS
        auto sEngine = ScriptEngineManager::getInstance()->getScriptEngine();
        if (sEngine)
        {
            sEngine->releaseScriptObject(this, _runningScene);
        }
#endif // CC_ENABLE_GC_FOR_NATIVE_OBJECTS
        _runningScene->release();
    }
    _runningScene = _nextScene;
    _runningScene->retain();
    _nextScene = nullptr;

    if ((! runningIsTransition) && _runningScene)
    {
        _runningScene->onEnter();
        _runningScene->onEnterTransitionDidFinish();
    }
}

void Director::pause()
{
    if (_paused)
    {
        return;
    }

    _oldAnimationInterval = _animationInterval;

    // when paused, don't consume CPU
    setAnimationInterval(1 / 4.0);
    _paused = true;
}

void Director::resume()
{
    if (! _paused)
    {
        return;
    }

    setAnimationInterval(_oldAnimationInterval);

    _paused = false;
    //_deltaTime = 0;
    // fix issue #3509, skip one fps to avoid incorrect time calculation.
    setNextDeltaTimeZero(true);
}

// display the FPS using a LabelAtlas
// updates the FPS every frame
void Director::showStats()
{
    bgfx::setDebug(BGFX_DEBUG_TEXT);
    bgfx::dbgTextClear();
    const bgfx::Stats* stats = bgfx::getStats();
    const char* rendererNames[] =
    {
        "Noop",
        "Direct3D9",
        "Direct3D11",
        "Direct3D12",
        "Gnm",
        "Metal",
        "OpenGLES",
        "OpenGL",
        "Vulkan"
    };
    uint8_t dbgViewId = SharedView.getId();
    int32_t row = 0;
    bgfx::dbgTextPrintf(dbgViewId, ++row, 0x0f, "\x1b[33;mRenderer: \x1b[63;m%s", rendererNames[bgfx::getCaps()->rendererType]);
    bgfx::dbgTextPrintf(dbgViewId, ++row, 0x0f, "\x1b[33;mMultithreaded: \x1b[63;m%s", (bgfx::getCaps()->supported & BGFX_CAPS_RENDERER_MULTITHREADED) ? "true" : "false");
    Size size = _openGLView->getViewPortRect().size;
    bgfx::dbgTextPrintf(dbgViewId, ++row, 0x0f, "\x1b[33;mBackbuffer: \x1b[63;m%d x %d", static_cast<int32_t>(size.width), static_cast<int32_t>(size.height));
    bgfx::dbgTextPrintf(dbgViewId, ++row, 0x0f, "\x1b[33;mDraw call: \x1b[63;m%d", stats->numDraw);
    static int32_t frames = 0;
    static double cpuTime = 0, gpuTime = 0, deltaTime = 0;
    cpuTime += SharedApplication.getCPUTime();
    gpuTime += std::abs(double(stats->gpuTimeEnd) - double(stats->gpuTimeBegin)) / double(stats->gpuTimerFreq);
    deltaTime += SharedApplication.getDeltaTime();
    frames++;
    static double lastCpuTime = 0, lastGpuTime = 0, lastDeltaTime = 1000.0 / SharedApplication.getMaxFPS();
    bgfx::dbgTextPrintf(dbgViewId, ++row, 0x0f, "\x1b[33;mCPU time: \x1b[63;m%.1f ms", lastCpuTime);
    if (lastGpuTime > 0.0)
    {
        bgfx::dbgTextPrintf(dbgViewId, ++row, 0x0f, "\x1b[33;mGPU time: \x1b[63;m%.1f ms", lastGpuTime);
    }
    bgfx::dbgTextPrintf(dbgViewId, ++row, 0x0f, "\x1b[33;mDelta time: \x1b[63;m%.1f ms", lastDeltaTime);
    bgfx::dbgTextPrintf(dbgViewId, ++row, 0x0f, "\x1b[33;mFPS: \x1b[63;m%d",static_cast<int32_t>(1000.0f / lastDeltaTime));
    if (frames == SharedApplication.getMaxFPS())
    {
        lastCpuTime = 1000.0 * cpuTime / frames;
        lastGpuTime = 1000.0 * gpuTime / frames;
        lastDeltaTime = 1000.0 * deltaTime / frames;
        frames = 0;
        cpuTime = gpuTime = deltaTime = 0.0;
    }
}

void Director::calculateMPF()
{
    static float prevSecondsPerFrame = 0;
    static const float MPF_FILTER = 0.10f;

    auto now = std::chrono::steady_clock::now();

    _secondsPerFrame = std::chrono::duration_cast<std::chrono::microseconds>(now - _lastUpdate).count() / 1000000.0f;

    _secondsPerFrame = _secondsPerFrame * MPF_FILTER + (1-MPF_FILTER) * prevSecondsPerFrame;
    prevSecondsPerFrame = _secondsPerFrame;
}

// returns the FPS image data pointer and len
void Director::getFPSImageData(unsigned char** datapointer, ssize_t* length)
{
    // FIXME: fixed me if it should be used
    *datapointer = cc_fps_images_png;
    *length = cc_fps_images_len();
}

void Director::setContentScaleFactor(float scaleFactor)
{
    if (scaleFactor != _contentScaleFactor)
    {
        _contentScaleFactor = scaleFactor;
        _isStatusLabelUpdated = true;
    }
}

void Director::setNotificationNode(Node *node)
{
    if (_notificationNode != nullptr){
        _notificationNode->onExitTransitionDidStart();
        _notificationNode->onExit();
        _notificationNode->cleanup();
    }
    CC_SAFE_RELEASE(_notificationNode);

    _notificationNode = node;
    if (node == nullptr)
        return;
    _notificationNode->onEnter();
    _notificationNode->onEnterTransitionDidFinish();
    CC_SAFE_RETAIN(_notificationNode);
}

void Director::setScheduler(Scheduler* scheduler)
{
    if (_scheduler != scheduler)
    {
        _scheduler = scheduler;
    }
}

void Director::setActionManager(ActionManager* actionManager)
{
    if (_actionManager != actionManager)
    {
        _actionManager = actionManager;
    }
}

void Director::setEventDispatcher(EventDispatcher* dispatcher)
{
    if (_eventDispatcher != dispatcher)
    {
        _eventDispatcher = dispatcher;
    }
}

void Director::startAnimation()
{
    _lastUpdate = std::chrono::steady_clock::now();

    _invalid = false;

    _cocos2d_thread_id = std::this_thread::get_id();

    SharedApplication.setAnimationInterval(_animationInterval);

    // fix issue #3509, skip one fps to avoid incorrect time calculation.
    setNextDeltaTimeZero(true);
}

void Director::mainLoop()
{
    if (_purgeDirectorInNextLoop)
    {
        _purgeDirectorInNextLoop = false;
        purgeDirector();
    }
    else if (_restartDirectorInNextLoop)
    {
        _restartDirectorInNextLoop = false;
        restartDirector();
    }
    else if (! _invalid)
    {
        drawScene();

        // release the objects
        PoolManager::getInstance()->getCurrentPool()->clear();
    }
}

void Director::stopAnimation()
{
    _invalid = true;
}

void Director::setAnimationInterval(float interval)
{
    _animationInterval = interval;
    if (! _invalid)
    {
        stopAnimation();
        startAnimation();
    }
}

void Director::setDisplayStats(bool displayStats)
{
    _displayStats = displayStats;
    if (!displayStats)
    {
        bgfx::setDebug(BGFX_DEBUG_NONE);
    }
}

NS_CC_END

