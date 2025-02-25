#include "input.hpp"

#include <cassert>

Input::Input()
{
    controls[(int)Key::Up]    = { SDL_SCANCODE_W,      SDL_SCANCODE_UP,    SDL_SCANCODE_UNKNOWN };
    controls[(int)Key::Left]  = { SDL_SCANCODE_A,      SDL_SCANCODE_LEFT,  SDL_SCANCODE_UNKNOWN };
    controls[(int)Key::Down]  = { SDL_SCANCODE_S,      SDL_SCANCODE_DOWN,  SDL_SCANCODE_UNKNOWN };
    controls[(int)Key::Right] = { SDL_SCANCODE_D,      SDL_SCANCODE_RIGHT, SDL_SCANCODE_UNKNOWN };
    controls[(int)Key::Jump]  = { SDL_SCANCODE_W,      SDL_SCANCODE_UP,    SDL_SCANCODE_SPACE };
    controls[(int)Key::Boost] = { SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT, SDL_SCANCODE_UNKNOWN };
}

Math::float2 Input::Joystick() const
{
    Math::float2 joystick = 0.0f;
    if (IsKeyDown(Key::Up))    joystick.y += 1.0f;
    if (IsKeyDown(Key::Left))  joystick.x -= 1.0f;
    if (IsKeyDown(Key::Down))  joystick.y -= 1.0f;
    if (IsKeyDown(Key::Right)) joystick.x += 1.0f;
    return joystick;
}

bool Input::IsKeyDown(Key key) const
{
    for (SDL_Scancode scancode : controls[(int)key])
    {
        if (IsKeyDown(scancode))
        {
            return true;
        }
    }
    return false;
}

bool Input::IsKeyPressed(Key key) const
{
    for (SDL_Scancode scancode : controls[(int)key])
    {
        if (IsKeyPressed(scancode))
        {
            return true;
        }
    }
    return false;
}

SDL_Scancode Input::GetFirstKeyPressed() const
{
    return m_FirstKeyPressed;
}

Math::float2 Input::GetMousePosition() const
{
    return m_MousePosition;
}

bool Input::IsMouseDown() const
{
    return m_MouseDownState & SDL_BUTTON_LMASK;
}

bool Input::IsMousePressed() const
{
    return m_MousePressedState & SDL_BUTTON_LMASK;
}

void Input::OnEvent(const SDL_Event& event)
{
    switch (event.type)
    {
        case SDL_EVENT_KEY_DOWN:
            assert(event.key.scancode >= 0 && event.key.scancode <= m_KeysDown.size());
            if (!event.key.repeat)
            {
                m_KeysDown[event.key.scancode] = 1;
                m_KeysPressed[event.key.scancode] = 1;
                m_FirstKeyPressed = event.key.scancode;
            }
            break;
        case SDL_EVENT_KEY_UP:
            assert(event.key.scancode >= 0 && event.key.scancode <= m_KeysDown.size());
            m_KeysDown[event.key.scancode] = 0;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            uint8_t mask = SDL_BUTTON_MASK(event.button.button);
            m_MouseDownState |= mask;
            m_MousePressedState |= mask;
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP:
            m_MouseDownState &= ~SDL_BUTTON_MASK(event.button.button);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            m_MousePosition = { event.motion.x, event.motion.y };
            break;
    }
}

void Input::EndFrame()
{
    m_KeysPressed.reset();
    m_FirstKeyPressed = SDL_SCANCODE_UNKNOWN;
    m_MousePressedState = 0;
}

bool Input::IsKeyDown(SDL_Scancode scancode) const
{
    assert(scancode >= 0 && scancode <= m_KeysDown.size());
    return m_KeysDown[scancode];
}

bool Input::IsKeyPressed(SDL_Scancode scancode) const
{
    assert(scancode >= 0 && scancode < m_KeysPressed.size());
    return m_KeysPressed[scancode];
}
