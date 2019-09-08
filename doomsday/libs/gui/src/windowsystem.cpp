/** @file windowsystem.cpp  Window management subsystem.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/WindowSystem"
#include "de/GuiRootWidget"
#include "de/Style"
#include "de/BaseGuiApp" // for updating pixel ratio

#include <de/Map>

#include <SDL_events.h>

namespace de {

DE_PIMPL(WindowSystem)
, DE_OBSERVES(GLWindow, PixelRatio)
{
    struct WindowData {
        BaseWindow *window;
        duint32     nativeId = ~0u;

        WindowData(BaseWindow *win)
            : window(win)
        {
            nativeId = SDL_GetWindowID(reinterpret_cast<SDL_Window *>(window->sdlWindow()));
        }
    };

    Map<String, WindowData> windows;
    String                  focusedId; // name of focused window (receives gesture input)
    std::unique_ptr<Style>  style;

    Impl(Public *i)
        : Base(i)
    {
        // Create a blank style by default.
        setStyle(new Style);
    }

    ~Impl()
    {
        self().closeAll();
    }

    void setStyle(Style *s)
    {
        style.reset(s);
        Style::setAppStyle(*s); // use as global style
    }

#if 0
    void processLatestMousePositionsIfMoved()
    {
        for (auto &entry : windows)
        {
            auto &winData = entry.second;
            if (winData.mouseMoved)
            {
                winData.mouseMoved = false;
                winData.processLatestMousePosition();
            }
        }
    }
#endif

    void windowPixelRatioChanged(GLWindow &win)
    {
        /// @todo Different windows may have different pixel ratios.
        DE_BASE_GUI_APP->setPixelRatio(float(win.pixelRatio()));
    }

    BaseWindow *findWindow(duint32 sdlId) const
    {
        for (const auto &w : windows)
        {
            if (w.second.nativeId == sdlId)
            {
                return w.second.window;
            }
        }
        // DE_ASSERT_FAIL("cannot find window -- invalid window ID")
        return nullptr;
    }

    void handleSDLEvent(GLWindow &window, const SDL_Event &event)
    {
        /// @todo Handle game controller events, too.

        window.glActivate();

        switch (event.type)
        {
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            case SDL_TEXTINPUT:
            case SDL_MOUSEMOTION:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEWHEEL:
            case SDL_FINGERUP:
            case SDL_FINGERDOWN:
            case SDL_MULTIGESTURE:
                window.eventHandler().handleSDLEvent(&event);
                break;

            case SDL_WINDOWEVENT:
                window.handleWindowEvent(&event);
                break;

            default: break;
        }
    }

    WindowData *findData(const BaseWindow &window)
    {
        auto found = windows.find(window.id());
        if (found == windows.end())
        {
            return nullptr;
        }
        return &found->second;
    }
};

WindowSystem::WindowSystem()
    : System(ObservesTime /*| ReceivesInputEvents*/)
    , d(new Impl(this))
{}

void WindowSystem::setStyle(Style *style)
{
    d->setStyle(style);
}

void WindowSystem::addWindow(const String &id, BaseWindow *window)
{
    window->audienceForPixelRatio() += d;
    d->windows.insert(id, {window});
    setFocusedWindow(id); // autofocus latest
}

bool WindowSystem::mainExists() // static
{
    return BaseWindow::mainExists();
}

BaseWindow &WindowSystem::main() // static
{
    DE_ASSERT(mainExists());
    return static_cast<BaseWindow &>(GLWindow::getMain());
}

void WindowSystem::setFocusedWindow(const String &id)
{
    d->focusedId = id;
}

BaseWindow *WindowSystem::focusedWindow() // static
{
    return get().find(get().d->focusedId);
}

dsize WindowSystem::count() const
{
    return d->windows.size();
}

BaseWindow *WindowSystem::find(const String &id) const
{
    auto found = d->windows.find(id);
    if (found != d->windows.end())
    {
        return found->second.window;
    }
    return nullptr;
}

LoopResult WindowSystem::forAll(const std::function<LoopResult (BaseWindow *)>& func)
{
    for (const auto &win : d->windows)
    {
        win.second.window->glActivate();
        if (auto result = func(win.second.window))
        {
            return result;
        }
    }
    return LoopContinue;
}

void WindowSystem::closeAll()
{
    closingAllWindows();

    for (const auto &w : d->windows)
    {
        delete w.second.window;
    }
    d->windows.clear();
}

Style &WindowSystem::style()
{
    return *d->style;
}

#if 0
void WindowSystem::dispatchLatestMousePosition(BaseWindow &window)
{
    if (auto *wd = d->findData(window))
    {
        wd->processLatestMousePosition();
    }
}

Vec2i WindowSystem::latestMousePosition(const BaseWindow &window) const
{
    if (auto *wd = d->findData(window))
    {
        return wd->latestMousePos;
    }
    return {};
}
#endif

#if 0
bool WindowSystem::processEvent(const Event &event)
{
    /*
     * Mouse motion is filtered as it may be produced needlessly often with
     * high-frequency mice. Note that this does not affect DirectInput mouse
     * input at all (however, as DirectInput is polled once per frame, it is
     * already filtered anyway).
     */

    if (event.type() == Event::MousePosition)
    {
        const MouseEvent &mouse = event.as<MouseEvent>();

        if (mouse.pos() != d->latestMousePos)
        {
            // This event will be emitted later, before widget tree update.
            d->latestMousePos = mouse.pos();
            d->mouseMoved = true;
        }
        return true;
    }

    // Dispatch the event to the main window's widget tree.
    return rootProcessEvent(event);
}
#endif

void WindowSystem::timeChanged(const Clock &/*clock*/)
{
//    d->processLatestMousePositionsIfMoved();

    // Update periodically.
    rootUpdate();
}

static uint32_t getWindowId(const SDL_Event &event)
{
    switch (event.type)
    {
        case SDL_MOUSEMOTION: return event.motion.windowID;
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEBUTTONDOWN: return event.button.windowID;
        case SDL_MOUSEWHEEL: return event.wheel.windowID;
        case SDL_KEYDOWN:
        case SDL_KEYUP: return event.key.windowID;
        case SDL_TEXTINPUT: return event.text.windowID;
        case SDL_WINDOWEVENT: return event.window.windowID;
    }
    return ~0u;
}

void WindowSystem::pollAndDispatchEvents()
{
    try
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_QUIT:
                    DE_GUI_APP->quit(0);
                    break;

                /// @todo Handle game controller events, too.

                case SDL_WINDOWEVENT:
                case SDL_MOUSEMOTION:
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                case SDL_MOUSEWHEEL:
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                case SDL_TEXTINPUT:
                    // These events are specific to one window.
                    if (auto *win = d->findWindow(getWindowId(event)))
                    {
                        d->handleSDLEvent(*win, event);
                    }
                    break;

                case SDL_MULTIGESTURE:
                case SDL_FINGERUP:
                case SDL_FINGERDOWN:
                    // Gestures affect the currently focused window.
                    if (auto *win = focusedWindow())
                    {
                        d->handleSDLEvent(*win, event);
                    }
                    break;
            }
        }
    }
    catch (const Error &er)
    {
        LOG_WARNING("Uncaught error during event processing: %s") << er.asText();
    }
}

void WindowSystem::closingAllWindows()
{}

//bool WindowSystem::rootProcessEvent(const Event &event)
//{
//    if (auto *win = focusedWindow())
//    {
//        return win->root().processEvent(event);
//    }
//    return false;
//}

void WindowSystem::rootUpdate()
{
    for (const auto &win : d->windows)
    {
        auto *window = win.second.window;
        window->glActivate();
        window->processLatestMousePosition();
        window->root().update();
    }
}

static WindowSystem *theAppWindowSystem = nullptr;

void WindowSystem::setAppWindowSystem(WindowSystem &winSys)
{
    theAppWindowSystem = &winSys;
}

WindowSystem &WindowSystem::get() // static
{
    DE_ASSERT(theAppWindowSystem != nullptr);
    return *theAppWindowSystem;
}

} // namespace de
