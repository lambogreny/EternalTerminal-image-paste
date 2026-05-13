#ifndef __ET_LOCAL_CLIPBOARD_IMAGE_HPP__
#define __ET_LOCAL_CLIPBOARD_IMAGE_HPP__

#include "ClipboardImageFrame.hpp"

namespace et {

constexpr const char* kLocalClipboardPasteTrigger = "\x1b]777;et-paste\a";

optional<ClipboardImagePayload> readLocalClipboardImage();
optional<string> readLocalClipboardText();

}  // namespace et

#endif  // __ET_LOCAL_CLIPBOARD_IMAGE_HPP__
