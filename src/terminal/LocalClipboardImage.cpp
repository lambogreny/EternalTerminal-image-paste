#include "LocalClipboardImage.hpp"

namespace et {
namespace {

#ifdef __APPLE__

const char* kReadClipboardImageScript = R"JXA(
ObjC.import('AppKit');
ObjC.import('Foundation');
ObjC.import('stdlib');

function writeData(data, extension) {
  const path = '/tmp/et-clipboard-' + $.NSUUID.UUID.UUIDString.js + '.' + extension;
  if (!data.writeToFileAtomically(path, true)) {
    $.exit(3);
  }
  const line = $(path + '\t' + extension + '\n').dataUsingEncoding(
    $.NSUTF8StringEncoding
  );
  $.NSFileHandle.fileHandleWithStandardOutput.writeData(line);
  $.exit(0);
}

const pasteboard = $.NSPasteboard.generalPasteboard;
const nativeTypes = [
  ['public.png', 'png'],
  ['public.jpeg', 'jpg'],
  ['com.compuserve.gif', 'gif']
];

for (const pair of nativeTypes) {
  const data = pasteboard.dataForType(pair[0]);
  if (data && data.length > 0) {
    writeData(data, pair[1]);
  }
}

const image = $.NSImage.alloc.initWithPasteboard(pasteboard);
if (image && image.isValid) {
  const tiff = image.TIFFRepresentation;
  if (tiff) {
    const bitmap = $.NSBitmapImageRep.imageRepWithData(tiff);
    if (bitmap) {
      const png = bitmap.representationUsingTypeProperties(
        $.NSBitmapImageFileTypePNG,
        $.NSDictionary.dictionary
      );
      if (png && png.length > 0) {
        writeData(png, 'png');
      }
    }
  }
}

const tiff = pasteboard.dataForType('public.tiff');
if (tiff && tiff.length > 0) {
  writeData(tiff, 'tiff');
}

$.exit(2);
)JXA";

const char* kReadClipboardTextScript = R"JXA(
ObjC.import('AppKit');
ObjC.import('Foundation');
ObjC.import('stdlib');

const pasteboard = $.NSPasteboard.generalPasteboard;
const text = pasteboard.stringForType($.NSPasteboardTypeString);
if (!text || text.length === 0) {
  $.exit(2);
}

const data = text.dataUsingEncoding($.NSUTF8StringEncoding);
if (!data || data.length === 0) {
  $.exit(2);
}

$.NSFileHandle.fileHandleWithStandardOutput.writeData(data);
$.exit(0);
)JXA";

string shellQuote(const string& value) {
  string quoted = "'";
  for (char c : value) {
    if (c == '\'') {
      quoted += "'\\''";
    } else {
      quoted += c;
    }
  }
  quoted += "'";
  return quoted;
}

optional<pair<string, string>> runClipboardImageScript() {
  string command = "/usr/bin/osascript -l JavaScript -e " +
                   shellQuote(kReadClipboardImageScript);
  FILE* pipe = popen(command.c_str(), "r");
  if (!pipe) {
    return nullopt;
  }

  string output;
  char buffer[512];
  while (fgets(buffer, sizeof(buffer), pipe)) {
    output += buffer;
  }

  int status = pclose(pipe);
  if (status != 0) {
    return nullopt;
  }

  while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
    output.pop_back();
  }

  size_t tab = output.find('\t');
  if (tab == string::npos || tab == 0 || tab + 1 >= output.size()) {
    return nullopt;
  }

  return make_pair(output.substr(0, tab), output.substr(tab + 1));
}

optional<string> runClipboardTextScript() {
  string command = "/usr/bin/osascript -l JavaScript -e " +
                   shellQuote(kReadClipboardTextScript);
  FILE* pipe = popen(command.c_str(), "r");
  if (!pipe) {
    return nullopt;
  }

  string output;
  array<char, 4096> buffer;
  while (true) {
    size_t count = fread(buffer.data(), 1, buffer.size(), pipe);
    if (count > 0) {
      output.append(buffer.data(), count);
    }
    if (count < buffer.size()) {
      if (feof(pipe)) {
        break;
      }
      if (ferror(pipe)) {
        pclose(pipe);
        return nullopt;
      }
    }
  }

  int status = pclose(pipe);
  if (status != 0 || output.empty()) {
    return nullopt;
  }

  return output;
}

#endif

}  // namespace

optional<ClipboardImagePayload> readLocalClipboardImage() {
#ifdef __APPLE__
  optional<pair<string, string>> scriptResult = runClipboardImageScript();
  if (!scriptResult) {
    return nullopt;
  }

  const string& path = scriptResult->first;
  const string& extension = scriptResult->second;
  ifstream input(path, ios::binary);
  if (!input) {
    ::remove(path.c_str());
    return nullopt;
  }

  input.seekg(0, ios::end);
  const streamoff byteCount = input.tellg();
  if (byteCount <= 0 ||
      static_cast<uint64_t>(byteCount) > kMaxClipboardImageBytes) {
    ::remove(path.c_str());
    return nullopt;
  }
  input.seekg(0, ios::beg);

  string bytes((istreambuf_iterator<char>(input)), istreambuf_iterator<char>());
  ::remove(path.c_str());
  if (bytes.empty()) {
    return nullopt;
  }

  return ClipboardImagePayload{extension, bytes};
#else
  return nullopt;
#endif
}

optional<string> readLocalClipboardText() {
#ifdef __APPLE__
  optional<string> text = runClipboardTextScript();
  if (!text || text->empty() || text->size() > kMaxClipboardImageBytes) {
    return nullopt;
  }

  return text;
#else
  return nullopt;
#endif
}

}  // namespace et
