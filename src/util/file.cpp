#include "file.h"
#include "util/log.h"
#include <cstring>
#include <memory>

namespace VSTIR {

    const char* FileExtension(const char* path) {
        const char* dot = strrchr(path, '.');
        const char* slash1 = strrchr(path, '/');
        const char* slash2 = strrchr(path, '\\');
        const char* slash = slash1 > slash2 ? slash1 : slash2;
        if (!dot || (slash && dot < slash)) return nullptr;
        return dot + 1;
    }

    FileType VFILE::GetFileType(const char* path) {
        const char* extension = FileExtension(path);
        if (strcmp(extension, "obj") == 0 || strcmp(extension, "OBJ") == 0) {
            return DOTOBJ;
        } else if (strcmp(extension, "spv") == 0 || strcmp(extension, "SPV") == 0) {
            return DOTSPV;
        } else if (strcmp(extension, "mtl") == 0 || strcmp(extension, "MTL") == 0) {
            return DOTMTL;
        } else if (strcmp(extension, "xml") == 0 || strcmp(extension, "XML") == 0) {
            return DOTXML;
        }
        return UNKNOWN;
    }

    char* VFILE::StripFilename(char* path) {
        for (int i = (int)strlen(path) - 1; i >= 0; i--) {
            if (path[i] == '/' || path[i] == '\\') {
                if (i == (int)strlen(path) - 1) return nullptr;
                return path + i + 1;
            }
        }
        return nullptr;
    }

    SimpleFile* VFILE::ReadFile(const char* filename) {
    	SimpleFile* sfile = (SimpleFile*)calloc(1, sizeof(SimpleFile));
        sfile->type = GetFileType(filename);
    	FILE* file = fopen(filename, "rb");
        if (file == nullptr) {
            ERROR("Unable to open file \"%s\"", filename);
            free(sfile);
            return nullptr;
        }
    	fseek(file, 0, SEEK_END);
    	sfile->size = ftell(file);
    	rewind(file);
    	sfile->data = (char*)calloc(sfile->size, sizeof(char));
    	size_t read = fread(sfile->data, 1, sfile->size, file);
        fclose(file);
        if (read != sfile->size) {
            ERROR("Unable to read file \"%s\"", filename);
            FreeFile(sfile);
            return nullptr;
        }
    	return sfile;
    }

    void VFILE::FreeFile(SimpleFile* file) {
    	free(file->data);
    	free(file);
    }

    LineParser VFILE::Parser(SimpleFile* file) {
        return (LineParser){ file, 0, 0 };
    }

    bool VFILE::NextLine(LineParser* lp, char* buffer, size_t size) {
        if (lp->cursor >= lp->file->size) return false;
        memset(buffer, 0, size);
        int last_ind = -1;
        for (size_t i = lp->cursor; i < lp->file->size; i++) {
            if (lp->file->data[i] == '\n') {
                last_ind = (int)i;
                break;
            }
        }
        lp->line++;
        if (last_ind < 0) {
            memcpy(buffer, lp->file->data + lp->cursor, lp->file->size - lp->cursor);
            lp->cursor = lp->file->size;
            return true;
        }
        if (last_ind - lp->cursor > size) {
            WARN("line overflow detected when parsing for lines...");
            memcpy(buffer, lp->file->data + lp->cursor, size);
            lp->cursor += size;
            return true;
        }
        memcpy(buffer, lp->file->data + lp->cursor, last_ind - lp->cursor);
        lp->cursor = last_ind + 1;
        return true;
    }

}
