#include "renderer.h"
#include "core/editor.h"
#include "core/get.h"
#include "util/log.h"
#include "util/file.h"
#include "util/bvh.h"
#include "vulkan/vutil.h"
#include <vulkan/vulkan.h>
#include <cstring>

#define INVOCATION_GROUP_SIZE 256
#define MAX_MTLLIB_PATH_SIZE 1024
#define MAX_OBJ_PATH_SIZE 1024
#define MAX_OBJ_LINE_SIZE 2048
#define MAX_OBJ_ARG_SIZE 32
#define MAX_OBJ_NUM_ARGS 64
#define MAX_MTL_LINE_SIZE 2048
#define MAX_MTL_ARG_SIZE 32
#define MAX_MTL_NUM_ARGS 64

#define SET_MTL_FLOAT_FIELD(field, a) state->materials[state->materials.size() - 1].field = a;
#define SET_MTL_UINT_FIELD(field, a) state->materials[state->materials.size() - 1].field = a;
#define SET_MTL_VEC3_FIELD(field, x, y, z) { \
    state->materials[state->materials.size() - 1].field[0] = x; \
    state->materials[state->materials.size() - 1].field[1] = y; \
    state->materials[state->materials.size() - 1].field[2] = z; \
}

namespace VSTIR {

    typedef bool (*ParseFuncOBJ)(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t, StateOBJ*);
    typedef bool (*ParseFuncMTL)(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t, StateOBJ*);

    bool IsWhitespace(char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }

    size_t ParseLineArgsOBJ(const char line[MAX_OBJ_LINE_SIZE], char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE]) {
        int numargs = 0;
        int cursor = 0;
        while (IsWhitespace(line[cursor])) cursor++;
        for (int i = cursor; i < MAX_OBJ_LINE_SIZE && line[i] != 0 && line[i] != '#'; i++) {
            if (IsWhitespace(line[i])) {
                memcpy(lineargs[numargs], line + cursor, i - cursor);
                numargs++;
                while (IsWhitespace(line[i])) i++;
                cursor = i;
                if (line[cursor] == '#') break;
            }
        }
        if (line[cursor] != 0 && line[cursor] != '#') {
            memcpy(lineargs[numargs], line + cursor, strnlen(line, MAX_OBJ_LINE_SIZE) - cursor);
            numargs++;
        }
        return numargs;
    }

    bool ParseFloat(const char* str, float* value) {
        char *end;
        float result;
        errno = 0;
        if (!str || !value) return false;
        result = strtof(str, &end);
        if (end == str || errno == ERANGE) return false;
        while (isspace((unsigned char)*end)) end++;
        if (*end != '\0') return false;
        *value = result;
        return true;
    }

    bool ParseUInt(const char* str, uint32_t* value) {
        char *end;
        unsigned long result;
        errno = 0;
        if (!str || !value) return false;
        result = strtoul(str, &end, 10);
        if (end == str || errno == ERANGE) return false;
        while (isspace((unsigned char)*end)) end++;
        if (*end != '\0') return false;
        *value = (uint32_t)result;
        return true;
    }

    bool ParseLInt(const char* str, int64_t* value) {
        char *end;
        long long result;
        errno = 0;
        if (!str || !value) return false;
        result = strtoll(str, &end, 10);
        if (end == str || errno == ERANGE) return false;
        while (isspace((unsigned char)*end)) end++;
        if (*end != '\0') return false;
        *value = (int64_t)result;
        return true;
    }

    bool ParseTriplet(const char* str, int64_t* a, int64_t* b, int64_t* c, size_t* count) {
        char abuff[MAX_OBJ_ARG_SIZE] = { 0 };
        char bbuff[MAX_OBJ_ARG_SIZE] = { 0 };
        char cbuff[MAX_OBJ_ARG_SIZE] = { 0 };
        *count = 0;
        size_t ptr = 0;
        for (size_t i = 0; i < strlen(str); i++) {
            if (str[i] == '/') {
                if (*count == 0) {
                    memcpy(abuff, str + ptr, i - ptr);
                    ptr = i + 1;
                    *count = 1;
                } else if (*count == 1) {
                    memcpy(bbuff, str + ptr, i - ptr);
                    ptr = i + 1;
                    *count = 2;
                } else {
                    return false;
                }
            }
        }
        memcpy(*count == 0 ? abuff : (*count == 1 ? bbuff : cbuff), str + ptr, strlen(str) - ptr);
        *count += 1;
        bool success = ParseLInt(abuff, a);
        success &= (*a != 0);
        if (success && strlen(bbuff) > 0) {
            success &= ParseLInt(bbuff, b) & (*b != 0);
        } else {
            b = 0;
        }
        if (success && strlen(cbuff) > 0) {
            success &= ParseLInt(cbuff, c) & (*c != 0);
        } else {
            c = 0;
        }
        return success;
    }

    bool ParseMTL_illum(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        if (numargs != 2) {
            ERROR("Cannot parse illumination model (illum) without exactly 1 argument - detected %d instead", (int)numargs - 1);
            return false;
        }
        uint32_t a;
        if (!(ParseUInt(lineargs[1], &a))) {
            ERROR("Invalid illumination model (illum) - expected 1 unsigned integer and got \"%s\" instead", lineargs[1]);
            return false;
        }
        if (a > 10) {
            ERROR("Invalid illumination model (illum) - expected a value from 0 to 10, and got \"%s\" instead", lineargs[1]);
            return false;
        }
        SET_MTL_UINT_FIELD(model, a);
        return true;
    }

    bool ParseMTL_Ni(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        if (numargs != 2) {
            ERROR("Cannot parse index of refraction field (Ni) without exactly 1 argument - detected %d instead", (int)numargs - 1);
            return false;
        }
        float a;
        if (!(ParseFloat(lineargs[1], &a))) {
            ERROR("Invalid index of refraction field (Ni) - expected 1 float and got \"%s\" instead", lineargs[1]);
            return false;
        }
        SET_MTL_FLOAT_FIELD(ior, a);
        return true;
    }

    bool ParseMTL_Ns(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        if (numargs != 2) {
            ERROR("Cannot parse specular shininess field (Ns) without exactly 1 argument - detected %d instead", (int)numargs - 1);
            return false;
        }
        float a;
        if (!(ParseFloat(lineargs[1], &a))) {
            ERROR("Invalid specular shininess field (Ns) - expected 1 float and got \"%s\" instead", lineargs[1]);
            return false;
        }
        SET_MTL_FLOAT_FIELD(shiny, a);
        return true;
    }

    bool ParseMTL_Ke(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        if (numargs != 4) {
            ERROR("Cannot parse an emission field (Ke) without exactly 3 arguments - detected %d instead", (int)numargs - 1);
            return false;
        }
        float x, y, z;
        if (!(ParseFloat(lineargs[1], &x) && ParseFloat(lineargs[2], &y) && ParseFloat(lineargs[3], &z))) {
            ERROR("Invalid emission (Ke) fields - expected 3 floats and got \"%s %s %s\" instead", lineargs[1], lineargs[2], lineargs[3]);
            return false;
        }
        SET_MTL_VEC3_FIELD(emission, x, y, z);
        return true;
    }

    bool ParseMTL_Ka(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        if (numargs != 4) {
            ERROR("Cannot parse an ambience field (Ka) without exactly 3 arguments - detected %d instead", (int)numargs - 1);
            return false;
        }
        float x, y, z;
        if (!(ParseFloat(lineargs[1], &x) && ParseFloat(lineargs[2], &y) && ParseFloat(lineargs[3], &z))) {
            ERROR("Invalid ambience (Ka) fields - expected 3 floats and got \"%s %s %s\" instead", lineargs[1], lineargs[2], lineargs[3]);
            return false;
        }
        SET_MTL_VEC3_FIELD(ambient, x, y, z);
        return true;
    }

    bool ParseMTL_Kd(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        if (numargs != 4) {
            ERROR("Cannot parse a diffuse field (Kd) without exactly 3 arguments - detected %d instead", (int)numargs - 1);
            return false;
        }
        float x, y, z;
        if (!(ParseFloat(lineargs[1], &x) && ParseFloat(lineargs[2], &y) && ParseFloat(lineargs[3], &z))) {
            ERROR("Invalid diffuse (Kd) fields - expected 3 floats and got \"%s %s %s\" instead", lineargs[1], lineargs[2], lineargs[3]);
            return false;
        }
        SET_MTL_VEC3_FIELD(diffuse, x, y, z);
        return true;
    }

    bool ParseMTL_Ks(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        if (numargs != 4) {
            ERROR("Cannot parse a specular field (Ks) without exactly 3 arguments - detected %d instead", (int)numargs - 1);
            return false;
        }
        float x, y, z;
        if (!(ParseFloat(lineargs[1], &x) && ParseFloat(lineargs[2], &y) && ParseFloat(lineargs[3], &z))) {
            ERROR("Invalid specular (Ks) fields - expected 3 floats and got \"%s %s %s\" instead", lineargs[1], lineargs[2], lineargs[3]);
            return false;
        }
        SET_MTL_VEC3_FIELD(specular, x, y, z);
        return true;
    }

    bool ParseMTL_Tf(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        if (numargs != 4) {
            ERROR("Cannot parse an absorbtion field (Tf) without exactly 3 arguments - detected %d instead", (int)numargs - 1);
            return false;
        }
        float x, y, z;
        if (!(ParseFloat(lineargs[1], &x) && ParseFloat(lineargs[2], &y) && ParseFloat(lineargs[3], &z))) {
            ERROR("Invalid absorbtion (Tf) fields - expected 3 floats and got \"%s %s %s\" instead", lineargs[1], lineargs[2], lineargs[3]);
            return false;
        }
        SET_MTL_VEC3_FIELD(absorbtion, x, y, z);
        return true;
    }

    bool ParseMTL_Rd(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        if (numargs != 4) {
            ERROR("Cannot parse a dispersion field (Rd) without exactly 3 arguments - detected %d instead", (int)numargs - 1);
            return false;
        }
        float x, y, z;
        if (!(ParseFloat(lineargs[1], &x) && ParseFloat(lineargs[2], &y) && ParseFloat(lineargs[3], &z))) {
            ERROR("Invalid dispersion (Rd) fields - expected 3 floats and got \"%s %s %s\" instead", lineargs[1], lineargs[2], lineargs[3]);
            return false;
        }
        SET_MTL_VEC3_FIELD(dispersion, x, y, z);
        return true;
    }

    bool ParseMTL_d(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        // Not useful for prism right now, implement later if needed
        return true;
    }

    bool ParseMTL_newmtl(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        if (numargs != 2) {
            ERROR("Invalid format for newmtl arguments, must have only 1 argument - detected %d instead", (int)numargs - 1);
            return false;
        }
        char* name = (char*)calloc(strlen(lineargs[1]), sizeof(char));
        strcpy(name, lineargs[1]);
        state->mnames.push_back(std::string(name));
        Material m{};
        state->materials.push_back(m);
        return true;
    }

    ParseFuncMTL GetParserFromArgMTL(const char* header) {
        #define GETPARSER(h) if (strcmp(header, #h) == 0) return ParseMTL_##h;
        GETPARSER(newmtl);
        GETPARSER(illum);
        GETPARSER(Ke);
        GETPARSER(Ka);
        GETPARSER(Ks);
        GETPARSER(Kd);
        GETPARSER(Ns);
        GETPARSER(Ni);
        GETPARSER(Tf);
        GETPARSER(Rd);
        GETPARSER(d);
        #undef GETPARSER
        return nullptr;
    }

    bool ParseOBJ_mtllib(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        if (numargs != 2) {
            ERROR("Invalid format for mtllib arguments, must have only 1 argument - detected %d instead", (int)numargs - 1);
            return false;
        }
        char mtlloc[MAX_MTLLIB_PATH_SIZE] = { 0 };
        char mtlpath[MAX_MTLLIB_PATH_SIZE] = { 0 };
        strcpy(mtlloc, state->filepath.c_str());
        char* fnstart = (char*)VFILE::StripFilename(mtlloc);
        if (fnstart) fnstart[0] = 0;
        else mtlloc[0] = 0;
        sprintf(mtlpath, "%s%s", mtlloc, lineargs[1]);
        SimpleFile* file = VFILE::ReadFile(mtlpath);
        if (!file) {
            ERROR("Unable to load invalid filepath to mtllib \"%s\"", mtlpath);
            return false;
        }
        if (file->type != DOTMTL) {
            ERROR("\"%s\" is not a .mtl file. Unable to open it with the MTL loader", mtlpath);
            VFILE::FreeFile(file);
            return false;
        }
        LineParser parser = VFILE::Parser(file);
        char line[MAX_MTL_LINE_SIZE] = { 0 };
        bool failure = false;
        while (VFILE::NextLine(&parser, line, MAX_MTL_LINE_SIZE)) {
            char lineargs[MAX_MTL_NUM_ARGS][MAX_MTL_ARG_SIZE] = { 0 };
            size_t numargs = ParseLineArgsOBJ(line, lineargs);
            if (numargs > 0 && lineargs[0][0] != '#') {
                ParseFuncMTL p = GetParserFromArgMTL(lineargs[0]);
                if (p) { if ((failure = !p(lineargs, numargs, state))) { ERROR("%s:%d - Unable to parse this MTL field due to an error", mtlpath, (int)parser.line); break; }
                } else WARN("%s:%d - Unknown MTL property detected: \"%s\", skipping parsing this field...", mtlpath, (int)parser.line, lineargs[0]);
            }
        }
        VFILE::FreeFile(file);
        return !failure;
    }

    bool ParseOBJ_usemtl(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        if (numargs != 2) {
            ERROR("Invalid format for usemtl arguments, must have only 1 argument - detected %d instead", (int)numargs - 1);
            return false;
        }
        size_t ind = 0;
        for (size_t i = 0; i < state->mnames.size(); i++) {
            if (strcmp(state->mnames[i].c_str(), lineargs[1]) == 0) {
                ind = i;
                goto found;
            }
        }
        ERROR("No material of name \"%s\" found", lineargs[1]);
        return false;
        found:
        state->markers.push_back((UseMaterialMarker){ (uint32_t)state->faces.size(), (uint32_t)ind });
        return true;
    }

    bool ParseOBJ_v(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        if (numargs != 4) {
            ERROR("Cannot parse a vertex field without exactly 3 arguments - detected %d instead", (int)numargs - 1);
            return false;
        }
        glm::vec3 v;
        if (!(ParseFloat(lineargs[1], &(v[0])) && ParseFloat(lineargs[2], &(v[1])) && ParseFloat(lineargs[3], &(v[2])))) {
            ERROR("Invalid vertex fields - expected 3 floats and got \"%s %s %s\" instead", lineargs[1], lineargs[2], lineargs[3]);
            return false;
        }
        state->vertices.push_back(v);
        return true;
    }

    bool ParseOBJ_vn(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        if (numargs != 4) {
            ERROR("Cannot parse a vertex normal field without exactly 3 arguments - detected %d instead", (int)numargs - 1);
            return false;
        }
        glm::vec3 v;
        if (!(ParseFloat(lineargs[1], &(v[0])) && ParseFloat(lineargs[2], &(v[1])) && ParseFloat(lineargs[3], &(v[2])))) {
            ERROR("Invalid vertex normal fields - expected 3 floats and got \"%s %s %s\" instead", lineargs[1], lineargs[2], lineargs[3]);
            return false;
        }
        state->normals.push_back(v);
        return true;
    }

    bool ParseOBJ_vt(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        if (numargs != 3 && numargs != 4) {
            ERROR("Cannot parse a vertex texture field without 2-3 arguments - detected %d instead", (int)numargs - 1);
            return false;
        }
        float u, v; // ignoring w
        if (!(ParseFloat(lineargs[1], &u) && ParseFloat(lineargs[2], &v))) {
            ERROR("Invalid vertex texture fields - expected 2 floats and got \"%s %s\" instead", lineargs[1], lineargs[2]);
            return false;
        }
        state->uvs.push_back(glm::vec2(u, v));
        return true;
    }

    bool ParseOBJ_f(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        if (numargs != 4 && numargs != 5) {
            ERROR("Cannot parse a face field without exactly 3 or 4 arguments - detected %d instead", (int)numargs - 1);
            return false;
        }
        int64_t values[4][3] = { 0 };
        int count = -1;
        for (int i = 0; i < (numargs == 4 ? 3 : 4); i++) {
            size_t c;
            if (!ParseTriplet(lineargs[i + 1], &values[i][0], &values[i][1], &values[i][2], &c)) {
                ERROR("Unable to parse face field \"%s\" into valid indices", lineargs[i + 1]);
                return false;
            }
            if (count < 0) {
                count = (int)c;
            } else if (count != (int)c) {
                ERROR("Detected a mismatched count in face field indices - expected %d but got %d instead", count, (int)c);
                return false;
            }
        }
        for (int i = 0; i < 2; i++) {
            int zeros = 0;
            for (int j = 0; j < (numargs == 4 ? 3 : 4); j++) {
                if (values[j][i] == 0) zeros++;
            }
            if (zeros != 0 && zeros != (numargs == 4 ? 3 : 4)) {
                ERROR("Detected a mismatched format count in face field indices - vertex normals/textures should either be all defined or not at all");
                return false;
            }
        }
        for (int i = 0; i < (numargs == 4 ? 3 : 4); i++) {
            if (values[i][0] == 0) {
                ERROR("Invalid face field detected - there is no vertex 0");
                return false;
            }
            if (values[i][0] < 0) {
                values[i][0] = state->vertices.size() + values[i][0];
            } else {
                values[i][0]--;
            }
            if (values[i][1] < 0) {
                values[i][1] = state->uvs.size() + values[i][1];
            } else if (values[i][1] > 0) {
                values[i][1]--;
            } else {
                values[i][1] = (size_t)-1;
            }
            if (values[i][2] < 0) {
                values[i][2] = state->normals.size() + values[i][2];
            } else if (values[i][2] > 0) {
                values[i][2]--;
            } else {
                values[i][2] = (size_t)-1;
            }
        }
        state->faces.push_back((Face){
            (uint32_t)values[0][0], (uint32_t)values[1][0], (uint32_t)values[2][0],
            (uint32_t)values[0][1], (uint32_t)values[1][1], (uint32_t)values[2][1],
            (uint32_t)values[0][2], (uint32_t)values[1][2], (uint32_t)values[2][2],
            values[0][1] != -1, values[0][2] != -1
        });
        if (numargs == 5) state->faces.push_back((Face){
            (uint32_t)values[0][0], (uint32_t)values[2][0], (uint32_t)values[3][0],
            (uint32_t)values[0][1], (uint32_t)values[2][1], (uint32_t)values[3][1],
            (uint32_t)values[0][2], (uint32_t)values[2][2], (uint32_t)values[3][2],
            values[0][1] != -1, values[0][2] != -1
        });
        return true;
    }

    bool ParseOBJ_g(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        // Not useful right now, implement later if needed
        return true;
    }

    bool ParseOBJ_o(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        // Not useful right now, implement later if needed
        return true;
    }

    bool ParseOBJ_s(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
        // Not useful right now, implement later if needed
        return true;
    }

    ParseFuncOBJ GetParserFromArgOBJ(const char* header) {
        #define GETPARSER(h) if (strcmp(header, #h) == 0) return ParseOBJ_##h;
        GETPARSER(mtllib);
        GETPARSER(usemtl);
        GETPARSER(v);
        GETPARSER(vn);
        GETPARSER(vt);
        GETPARSER(f);
        GETPARSER(g);
        GETPARSER(o);
        GETPARSER(s);
        #undef GETPARSER
        return nullptr;
    }

    bool Renderer::ConstructOBJ(const StateOBJ state) {
        bool failure = false;
        for (size_t i = 0; i < state.faces.size(); i++) {
            if (state.faces[i].a >= state.vertices.size()) {
                ERROR("Face %d references an invalid vertex %d - there are only %d vertices available", (int)i, (int)state.faces[i].a, (int)state.vertices.size());
                failure = true;
            }
            if (state.faces[i].b >= state.vertices.size()) {
                ERROR("Face %d references an invalid vertex %d - there are only %d vertices available", (int)i, (int)state.faces[i].b, (int)state.vertices.size());
                failure = true;
            }
            if (state.faces[i].c >= state.vertices.size()) {
                ERROR("Face %d references an invalid vertex normal %d - there are only %d vertices available", (int)i, (int)state.faces[i].c, (int)state.vertices.size());
                failure = true;
            }
            if (state.faces[i].normals && state.faces[i].an >= state.normals.size()) {
                ERROR("Face %d references an invalid vertex normal %d - there are only %d normals available", (int)i, (int)state.faces[i].an, (int)state.normals.size());
                failure = true;
            }
            if (state.faces[i].normals && state.faces[i].bn >= state.normals.size()) {
                ERROR("Face %d references an invalid vertex normal %d - there are only %d normals available", (int)i, (int)state.faces[i].bn, (int)state.normals.size());
                failure = true;
            }
            if (state.faces[i].normals && state.faces[i].cn >= state.normals.size()) {
                ERROR("Face %d references an invalid vertex normal %d - there are only %d normals available", (int)i, (int)state.faces[i].cn, (int)state.normals.size());
                failure = true;
            }
        }
        if (failure) return false;
        std::vector<uint32_t> ids;
        size_t vertices_start = m_Geometry.vertices.size();
        size_t normals_start = m_Geometry.normals.size();
        for (size_t i = 0; i < state.materials.size(); i++) {
            ids.push_back(i);
            m_Geometry.materials.push_back(state.materials[i]);
        }
        for (size_t i = 0; i < state.vertices.size(); i++)
            m_Geometry.vertices.push_back(glm::vec4(state.vertices[i], 0));
        for (size_t i = 0; i < state.normals.size(); i++)
            m_Geometry.normals.push_back(glm::vec4(state.normals[i], 0));
        size_t current_marker = 0;
        uint32_t current_material = 0;
        for (size_t i = 0; i < state.faces.size(); i++) {
            while (current_marker < state.markers.size() && state.markers[current_marker].faceIndex <= i) {
                current_material = ids[state.markers[current_marker].materialIndex];
                current_marker++;
            }
            Triangle triangle = {
                (uint32_t)vertices_start + state.faces[i].a,
                (uint32_t)vertices_start + state.faces[i].b,
                (uint32_t)vertices_start + state.faces[i].c,
                state.normals.size() > 0 && state.faces[i].normals ? (uint32_t)normals_start + state.faces[i].an : (uint32_t)-1,
                state.normals.size() > 0 && state.faces[i].normals ? (uint32_t)normals_start + state.faces[i].bn : (uint32_t)-1,
                state.normals.size() > 0 && state.faces[i].normals ? (uint32_t)normals_start + state.faces[i].cn : (uint32_t)-1,
                current_material
            };
            m_Geometry.triangles.push_back(triangle);
        }
        return true;
    }

    void Renderer::Initialize() {
        m_Backend.Initialize();
        m_Camera = (Camera){
            glm::vec3(0.0f, 2.133f, 2.11f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            90.0f};
    }

    void Renderer::Render() {
        uint32_t imageIndex;
        vkAcquireNextImageKHR(
            _interface, _context.Swapchain().swapchain, UINT64_MAX,
            _scheduler.Syncro().imageAvailable, VK_NULL_HANDLE, &imageIndex);

        vkResetCommandBuffer(_scheduler.Commands().command, 0);
        _data.UpdateUBOs();
        RecordCommand(imageIndex);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &_scheduler.Syncro().imageAvailable;
        submitInfo.pWaitDstStageMask    = &waitStage;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &_scheduler.Commands().command;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &_scheduler.Syncro().renderFinished;
        vkQueueSubmit(_scheduler.Queue(), 1, &submitInfo, VK_NULL_HANDLE);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &_scheduler.Syncro().renderFinished;
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &_context.Swapchain().swapchain;
        presentInfo.pImageIndices      = &imageIndex;
        vkQueuePresentKHR(_scheduler.Queue(), &presentInfo);
        vkQueueWaitIdle(_scheduler.Queue());
    }

    void Renderer::LoadScene(std::string filepath) {
        vkDeviceWaitIdle(_interface);
        SimpleFile* file = VFILE::ReadFile(filepath.c_str());
        uint32_t startv = m_Geometry.vertices.size();
        if (!file) FATAL("Unable to load invalid filepath \"%s\"", filepath);
        if (file->type != DOTOBJ) FATAL("\"%s\" is not a .obj file. Unable to open it with the OBJ loader", filepath);
        LineParser parser = VFILE::Parser(file);
        StateOBJ state{};
        state.filepath = std::string(filepath);
        char line[MAX_OBJ_LINE_SIZE] = { 0 };
        while (VFILE::NextLine(&parser, line, MAX_OBJ_LINE_SIZE)) {
            char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE] = { 0 };
            size_t numargs = ParseLineArgsOBJ(line, lineargs);
            if (numargs > 0 && lineargs[0][0] != '#') {
                ParseFuncOBJ p = GetParserFromArgOBJ(lineargs[0]);
                if (p) { if (!p(lineargs, numargs, &state)) { ERROR("%s:%d - Unable to parse this OBJ field due to an error", filepath.c_str(), (int)parser.line); VFILE::FreeFile(file); return; }
                } else WARN("%s:%d - Unknown OBJ property detected: \"%s\", skipping parsing this field...", filepath.c_str(), (int)parser.line, lineargs[0]);
            }
        }
        if (!ConstructOBJ(state)) FATAL("Unable to construct .obj \"%s\" due to an error", filepath);
        VFILE::FreeFile(file);
        m_Geometry.bvh = BVH::Create(m_Geometry.triangles, m_Geometry.vertices);
        m_Geometry.bvh_size = m_Geometry.bvh.size();
        m_Geometry.vertices_size = m_Geometry.vertices.size();
        m_Geometry.normals_size = m_Geometry.normals.size();
        m_Geometry.triangles_size = m_Geometry.triangles.size();
        m_Geometry.emissives_size = m_Geometry.emissives.size();
        m_Geometry.materials_size = m_Geometry.materials.size();
        m_Backend.Reconstruct();
    }

    void Renderer::RecordCommand(uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VkResult result = vkBeginCommandBuffer(_scheduler.Commands().command, &beginInfo);
        ASSERT(result == VK_SUCCESS, "Failed to begin recording command buffer!");

        // execute shader stages
        for (size_t i = 0; i < _shaders.size(); i++) {
            uint32_t invocations = _width * _height;
            vkCmdBindPipeline(
                _scheduler.Commands().command,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                _context.Pipeline().pipeline[i]);
            vkCmdBindDescriptorSets(
                _scheduler.Commands().command,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                _context.Pipeline().layout[i],
                0,
                1,
                &(_data.Descriptors()[i].set),
                0,
                nullptr);
            vkCmdDispatch(_scheduler.Commands().command, ceil((invocations) / ((float)INVOCATION_GROUP_SIZE)), 1, 1);
            VUTILS::RecordGeneralBarrier(_scheduler.Commands().command);
        }

        // Copy image to staging
        {
            VkImageMemoryBarrier imgBarrier{};
            imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imgBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            imgBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            imgBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            imgBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imgBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imgBarrier.image = _context.Target().image;
            imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imgBarrier.subresourceRange.baseMipLevel = 0;
            imgBarrier.subresourceRange.levelCount = 1;
            imgBarrier.subresourceRange.baseArrayLayer = 0;
            imgBarrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(
                _scheduler.Commands().command,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &imgBarrier);
            VkBufferImageCopy region{};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = (VkOffset3D){ 0, 0, 0 };
            region.imageExtent = (VkExtent3D){ (uint32_t)_width, (uint32_t)_height, 1 };
            vkCmdCopyImageToBuffer(
                _scheduler.Commands().command,
                _context.Target().image,
                VK_IMAGE_LAYOUT_GENERAL, _core.Bridge().buffer, 1, &region);
        }

        // Blit image
        {
            VkImage swapImg = _context.Swapchain().images[imageIndex];
            VkImageMemoryBarrier toTransferDst{};
            toTransferDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toTransferDst.srcAccessMask = 0;
            toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransferDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.image = swapImg;
            toTransferDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCmdPipelineBarrier(_scheduler.Commands().command,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &toTransferDst);
            VkImageBlit blit{};
            blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            blit.srcOffsets[0]  = { 0, 0, 0 };
            blit.srcOffsets[1]  = { (int32_t)_width, (int32_t)_height, 1 };
            blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            blit.dstOffsets[0]  = { 0, 0, 0 };
            blit.dstOffsets[1]  = { (int32_t)_width, (int32_t)_height, 1 };
            vkCmdBlitImage(_scheduler.Commands().command,
                _context.Target().image, VK_IMAGE_LAYOUT_GENERAL,
                swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit, VK_FILTER_NEAREST);
            VkImageMemoryBarrier toPresent = toTransferDst;
            toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toPresent.dstAccessMask = 0;
            toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            vkCmdPipelineBarrier(_scheduler.Commands().command,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0, 0, nullptr, 0, nullptr, 1, &toPresent);
        }

        // End command
        result = vkEndCommandBuffer(_scheduler.Commands().command);
        if (result != VK_SUCCESS) FATAL("Failed to record command!");
    }

}
