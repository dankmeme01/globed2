#include "keybinds.hpp"

#include <defs/geode.hpp>
#include <util/into.hpp>

using namespace geode::prelude;
using globed::Key;

namespace globed {
    std::string formatKey(Key key) {
#define KCASE(k) case Key::k: return #k
#define KCASEN(k, n) case Key::k: return n

        switch (key) {
            KCASE(A); KCASE(B); KCASE(C); KCASE(D); KCASE(E); KCASE(F); KCASE(G); KCASE(H); KCASE(I);
            KCASE(J); KCASE(K); KCASE(L); KCASE(M); KCASE(N); KCASE(O); KCASE(P); KCASE(Q); KCASE(R);
            KCASE(S); KCASE(T); KCASE(U); KCASE(V); KCASE(W); KCASE(X); KCASE(Y); KCASE(Z);

            KCASE(Space); KCASEN(Apostrophe, "'"); KCASEN(Comma, ","); KCASEN(Minus, "-"); KCASEN(Period, "."); KCASEN(Slash, "/"); KCASEN(Semicolon, ";"); KCASEN(Equal, "=");
            KCASEN(LeftBracket, "{"); KCASEN(RightBracket, "}"); KCASEN(Backslash, "\\"); KCASEN(Backtick, "`");

            KCASEN(Num0, "0"); KCASEN(Num1, "1"); KCASEN(Num2, "2"); KCASEN(Num3, "3"); KCASEN(Num4, "4");
            KCASEN(Num5, "5"); KCASEN(Num6, "6"); KCASEN(Num7, "7"); KCASEN(Num8, "8"); KCASEN(Num9, "9");

            KCASE(F1); KCASE(F2); KCASE(F3); KCASE(F4); KCASE(F5); KCASE(F6); KCASE(F7); KCASE(F8); KCASE(F9); KCASE(F10); KCASE(F11); KCASE(F12);
            KCASE(F13); KCASE(F14); KCASE(F15); KCASE(F16); KCASE(F17); KCASE(F18); KCASE(F19); KCASE(F20); KCASE(F21); KCASE(F22); KCASE(F23); KCASE(F24); KCASE(F25);

            KCASEN(NumPad0, "Num 0"); KCASEN(NumPad1, "Num 1"); KCASEN(NumPad2, "Num 2"); KCASEN(NumPad3, "Num 3"); KCASEN(NumPad4, "Num 4");
            KCASEN(NumPad5, "Num 5"); KCASEN(NumPad6, "Num 6"); KCASEN(NumPad7, "Num 7"); KCASEN(NumPad8, "Num 8"); KCASEN(NumPad9, "Num 9");
            KCASEN(NumPadDecimal, "Num ."); KCASEN(NumPadDivide, "Num /"); KCASEN(NumPadMultiply, "Num *");
            KCASEN(NumPadSubtract, "Num -"); KCASEN(NumPadAdd, "Num +"); KCASEN(NumPadEnter, "Num Enter"); KCASEN(NumPadEqual, "Num =");

            KCASE(Escape); KCASE(Enter); KCASE(Tab); KCASE(Backspace); KCASE(Insert); KCASE(Delete); KCASE(Home); KCASE(End);
            KCASEN(PageUp, "Page Up"); KCASEN(PageDown, "Page Down"); KCASEN(CapsLock, "Caps Lock"); KCASEN(ScrollLock, "Scroll Lock");
            KCASEN(NumLock, "Num Lock"); KCASEN(PrintScreen, "Print Screen"); KCASEN(Pause, "Pause");

            KCASE(Up); KCASE(Down); KCASE(Left); KCASE(Right);

            case Key::None: return "-";
        }
#undef KCASE
#undef KCASEN
    }
}

void KeybindsManager::handlePress(Key key) {
    if (key == Key::None) return;

    heldKeys[key] = true;
}

void KeybindsManager::handleRelease(Key key) {
    if (key == Key::None) return;

    heldKeys[key] = false;
}

bool KeybindsManager::isHeld(Key key) {
    return heldKeys.contains(key) && heldKeys.at(key);
}

#ifdef GEODE_IS_WINDOWS
static Key convertGlfwKey(int key) {
    switch (key) {
    case GLFW_KEY_A: return Key::A;
    case GLFW_KEY_B: return Key::B;
    case GLFW_KEY_C: return Key::C;
    case GLFW_KEY_D: return Key::D;
    case GLFW_KEY_E: return Key::E;
    case GLFW_KEY_F: return Key::F;
    case GLFW_KEY_G: return Key::G;
    case GLFW_KEY_H: return Key::H;
    case GLFW_KEY_I: return Key::I;
    case GLFW_KEY_J: return Key::J;
    case GLFW_KEY_K: return Key::K;
    case GLFW_KEY_L: return Key::L;
    case GLFW_KEY_M: return Key::M;
    case GLFW_KEY_N: return Key::N;
    case GLFW_KEY_O: return Key::O;
    case GLFW_KEY_P: return Key::P;
    case GLFW_KEY_Q: return Key::Q;
    case GLFW_KEY_R: return Key::R;
    case GLFW_KEY_S: return Key::S;
    case GLFW_KEY_T: return Key::T;
    case GLFW_KEY_U: return Key::U;
    case GLFW_KEY_V: return Key::V;
    case GLFW_KEY_W: return Key::W;
    case GLFW_KEY_X: return Key::X;
    case GLFW_KEY_Y: return Key::Y;
    case GLFW_KEY_Z: return Key::Z;
    case GLFW_KEY_SPACE: return Key::Space;
    case GLFW_KEY_APOSTROPHE: return Key::Apostrophe;
    case GLFW_KEY_COMMA: return Key::Comma;
    case GLFW_KEY_MINUS: return Key::Minus;
    case GLFW_KEY_PERIOD: return Key::Period;
    case GLFW_KEY_SLASH: return Key::Slash;
    case GLFW_KEY_SEMICOLON: return Key::Semicolon;
    case GLFW_KEY_EQUAL: return Key::Equal;
    case GLFW_KEY_LEFT_BRACKET: return Key::LeftBracket;
    case GLFW_KEY_BACKSLASH: return Key::Backslash;
    case GLFW_KEY_RIGHT_BRACKET: return Key::RightBracket;
    case GLFW_KEY_GRAVE_ACCENT: return Key::Backtick;
    case GLFW_KEY_0: return Key::Num0;
    case GLFW_KEY_1: return Key::Num1;
    case GLFW_KEY_2: return Key::Num2;
    case GLFW_KEY_3: return Key::Num3;
    case GLFW_KEY_4: return Key::Num4;
    case GLFW_KEY_5: return Key::Num5;
    case GLFW_KEY_6: return Key::Num6;
    case GLFW_KEY_7: return Key::Num7;
    case GLFW_KEY_8: return Key::Num8;
    case GLFW_KEY_9: return Key::Num9;
    case GLFW_KEY_F1: return Key::F1;
    case GLFW_KEY_F2: return Key::F2;
    case GLFW_KEY_F3: return Key::F3;
    case GLFW_KEY_F4: return Key::F4;
    case GLFW_KEY_F5: return Key::F5;
    case GLFW_KEY_F6: return Key::F6;
    case GLFW_KEY_F7: return Key::F7;
    case GLFW_KEY_F8: return Key::F8;
    case GLFW_KEY_F9: return Key::F9;
    case GLFW_KEY_F10: return Key::F10;
    case GLFW_KEY_F11: return Key::F11;
    case GLFW_KEY_F12: return Key::F12;
    case GLFW_KEY_F13: return Key::F13;
    case GLFW_KEY_F14: return Key::F14;
    case GLFW_KEY_F15: return Key::F15;
    case GLFW_KEY_F16: return Key::F16;
    case GLFW_KEY_F17: return Key::F17;
    case GLFW_KEY_F18: return Key::F18;
    case GLFW_KEY_F19: return Key::F19;
    case GLFW_KEY_F20: return Key::F20;
    case GLFW_KEY_F21: return Key::F21;
    case GLFW_KEY_F22: return Key::F22;
    case GLFW_KEY_F23: return Key::F23;
    case GLFW_KEY_F24: return Key::F24;
    case GLFW_KEY_F25: return Key::F25;
    case GLFW_KEY_KP_0: return Key::NumPad0;
    case GLFW_KEY_KP_1: return Key::NumPad1;
    case GLFW_KEY_KP_2: return Key::NumPad2;
    case GLFW_KEY_KP_3: return Key::NumPad3;
    case GLFW_KEY_KP_4: return Key::NumPad4;
    case GLFW_KEY_KP_5: return Key::NumPad5;
    case GLFW_KEY_KP_6: return Key::NumPad6;
    case GLFW_KEY_KP_7: return Key::NumPad7;
    case GLFW_KEY_KP_8: return Key::NumPad8;
    case GLFW_KEY_KP_9: return Key::NumPad9;
    case GLFW_KEY_KP_DECIMAL: return Key::NumPadDecimal;
    case GLFW_KEY_KP_DIVIDE: return Key::NumPadDivide;
    case GLFW_KEY_KP_MULTIPLY: return Key::NumPadMultiply;
    case GLFW_KEY_KP_SUBTRACT: return Key::NumPadSubtract;
    case GLFW_KEY_KP_ADD: return Key::NumPadAdd;
    case GLFW_KEY_KP_ENTER: return Key::NumPadEnter;
    case GLFW_KEY_KP_EQUAL: return Key::NumPadEqual;
    case GLFW_KEY_ESCAPE: return Key::Escape;
    case GLFW_KEY_ENTER: return Key::Enter;
    case GLFW_KEY_TAB: return Key::Tab;
    case GLFW_KEY_BACKSPACE: return Key::Backspace;
    case GLFW_KEY_INSERT: return Key::Insert;
    case GLFW_KEY_DELETE: return Key::Delete;
    case GLFW_KEY_HOME: return Key::Home;
    case GLFW_KEY_END: return Key::End;
    case GLFW_KEY_PAGE_UP: return Key::PageUp;
    case GLFW_KEY_PAGE_DOWN: return Key::PageDown;
    case GLFW_KEY_CAPS_LOCK: return Key::CapsLock;
    case GLFW_KEY_SCROLL_LOCK: return Key::ScrollLock;
    case GLFW_KEY_NUM_LOCK: return Key::NumLock;
    case GLFW_KEY_PRINT_SCREEN: return Key::PrintScreen;
    case GLFW_KEY_PAUSE: return Key::Pause;
    case GLFW_KEY_UP: return Key::Up;
    case GLFW_KEY_DOWN: return Key::Down;
    case GLFW_KEY_LEFT: return Key::Left;
    case GLFW_KEY_RIGHT: return Key::Right;
    default: return Key::None;
    }
}

// #include <Geode/modify/CCEGLView.hpp>

// class $modify(CCEGLView) {
//     void onGLFWKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
//         if (action == GLFW_PRESS) {
//             KeybindsManager::get().handlePress(convertGlfwKey(key));
//         } else if (action == GLFW_RELEASE) {
//             KeybindsManager::get().handleRelease(convertGlfwKey(key));
//         }

//         CCEGLView::onGLFWKeyCallback(window, key, scancode, action, mods);
//     }
// };

#else

Key convertCocosKey(enumKeyCodes key) {
    switch (key) {
        case KEY_A: return Key::A;
        case KEY_B: return Key::B;
        case KEY_C: return Key::C;
        case KEY_D: return Key::D;
        case KEY_E: return Key::E;
        case KEY_F: return Key::F;
        case KEY_G: return Key::G;
        case KEY_H: return Key::H;
        case KEY_I: return Key::I;
        case KEY_J: return Key::J;
        case KEY_K: return Key::K;
        case KEY_L: return Key::L;
        case KEY_M: return Key::M;
        case KEY_N: return Key::N;
        case KEY_O: return Key::O;
        case KEY_P: return Key::P;
        case KEY_Q: return Key::Q;
        case KEY_R: return Key::R;
        case KEY_S: return Key::S;
        case KEY_T: return Key::T;
        case KEY_U: return Key::U;
        case KEY_V: return Key::V;
        case KEY_W: return Key::W;
        case KEY_X: return Key::X;
        case KEY_Y: return Key::Y;
        case KEY_Z: return Key::Z;
        case KEY_Space: return Key::Space;
        //case KEY_Apostrophe: return Keys::Apostrophe;
        case KEY_OEMComma: return Key::Comma;
        case KEY_OEMMinus: return Key::Minus;
        case KEY_OEMPeriod: return Key::Period;
        //case KEY_Slash: return Keys::Slash;
        //case KEY_Semicolon: return Keys::Semicolon;
        //case KEY_Equal: return Keys::Equal;
        //case KEY_LeftBracket: return Keys::LeftBracket;
        //case KEY_BACKSLASH: return Keys::Backslash;
        //case KEY_RIGHT_BRACKET: return Keys::RightBracket;
        //case KEY_GRAVE_ACCENT: return Keys::GraveAccent;
        //case KEY_WORLD_1: return Keys::World1;
        //case KEY_WORLD_2: return Keys::World2;
        case KEY_Zero: return Key::Num0;
        case KEY_One: return Key::Num1;
        case KEY_Two: return Key::Num2;
        case KEY_Three: return Key::Num3;
        case KEY_Four: return Key::Num4;
        case KEY_Five: return Key::Num5;
        case KEY_Six: return Key::Num6;
        case KEY_Seven: return Key::Num7;
        case KEY_Eight: return Key::Num8;
        case KEY_Nine: return Key::Num9;
        case KEY_F1: return Key::F1;
        case KEY_F2: return Key::F2;
        case KEY_F3: return Key::F3;
        case KEY_F4: return Key::F4;
        case KEY_F5: return Key::F5;
        case KEY_F6: return Key::F6;
        case KEY_F7: return Key::F7;
        case KEY_F8: return Key::F8;
        case KEY_F9: return Key::F9;
        case KEY_F10: return Key::F10;
        case KEY_F11: return Key::F11;
        case KEY_F12: return Key::F12;
        case KEY_F13: return Key::F13;
        case KEY_F14: return Key::F14;
        case KEY_F15: return Key::F15;
        case KEY_F16: return Key::F16;
        case KEY_F17: return Key::F17;
        case KEY_F18: return Key::F18;
        case KEY_F19: return Key::F19;
        case KEY_F20: return Key::F20;
        case KEY_F21: return Key::F21;
        case KEY_F22: return Key::F22;
        case KEY_F23: return Key::F23;
        case KEY_F24: return Key::F24;
        //case KEY_F25: return Keys::F25;
        case KEY_NumPad0: return Key::NumPad0;
        case KEY_NumPad1: return Key::NumPad1;
        case KEY_NumPad2: return Key::NumPad2;
        case KEY_NumPad3: return Key::NumPad3;
        case KEY_NumPad4: return Key::NumPad4;
        case KEY_NumPad5: return Key::NumPad5;
        case KEY_NumPad6: return Key::NumPad6;
        case KEY_NumPad7: return Key::NumPad7;
        case KEY_NumPad8: return Key::NumPad8;
        case KEY_NumPad9: return Key::NumPad9;
        case KEY_Decimal: return Key::NumPadDecimal;
        case KEY_Divide: return Key::NumPadDivide;
        case KEY_Multiply: return Key::NumPadMultiply;
        case KEY_Subtract: return Key::NumPadSubtract;
        case KEY_Add: return Key::NumPadAdd;
        //case KEY_KP_ENTER: return Keys::NumPadEnter;
        //case KEY_Equal: return Keys::NumPadEqual;
        //case KEY_Menu: return Keys::Menu;
        case KEY_Escape: return Key::Escape;
        case KEY_Enter: return Key::Enter;
        case KEY_Tab: return Key::Tab;
        case KEY_Backspace: return Key::Backspace;
        case KEY_Insert: return Key::Insert;
        case KEY_Delete: return Key::Delete;
        case KEY_Home: return Key::Home;
        case KEY_End: return Key::End;
        case KEY_PageUp: return Key::PageUp;
        case KEY_PageDown: return Key::PageDown;
        case KEY_CapsLock: return Key::CapsLock;
        case KEY_ScrollLock: return Key::ScrollLock;
        case KEY_Numlock: return Key::NumLock;
        case KEY_PrintScreen: return Key::PrintScreen;
        case KEY_Pause: return Key::Pause;
        case KEY_ArrowUp: return Key::Up;
        case KEY_ArrowDown: return Key::Down;
        case KEY_ArrowLeft: return Key::Left;
        case KEY_ArrowRight: return Key::Right;
        default: return Key::None;
    }
}

// TODO: mac m1 crash?
// #include <Geode/modify/CCKeyboardDispatcher.hpp>

// class $modify(CCKeyboardDispatcher) {
//     bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat) {
//         if (down) {
//             KeybindsManager::get().handlePress(convertCocosKey(key));
//         } else if (!down) {
//             KeybindsManager::get().handleRelease(convertCocosKey(key));
//         }

//         return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat);
//     }
// };

#endif