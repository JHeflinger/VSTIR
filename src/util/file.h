#pragma once

#include <cstddef>
#include <cstdint>

namespace VSTIR {

    typedef enum {
        UNKNOWN = 0,
        DOTOBJ,
        DOTSPV,
        DOTMTL,
        DOTXML
    } FileType;

    typedef struct {
    	char* data;
    	size_t size;
        FileType type;
    } SimpleFile;

    typedef struct {
        SimpleFile* file;
        size_t line;
        size_t cursor;
    } LineParser;

    class VFILE {
    public:
        static FileType GetFileType(const char* path);
        static char* StripFilename(char* path);
        static SimpleFile* ReadFile(const char* filename);
        static void FreeFile(SimpleFile* file);
        static LineParser Parser(SimpleFile* file);
        static bool NextLine(LineParser* lp, char* buffer, size_t size);
    };

}
