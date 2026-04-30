#include "scene_loader.h"

#include "core/editor.h"
#include "util/bvh.h"
#include "util/file.h"
#include "util/log.h"

#include <yaml-cpp/yaml.h>

#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

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

namespace VSTIR::SceneLoader {

namespace {

typedef bool (*ParseFuncOBJ)(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t, StateOBJ*);
typedef bool (*ParseFuncMTL)(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t, StateOBJ*);

struct Transform {
	glm::vec3 translation = glm::vec3(0.0f);
	glm::vec3 rotationDegreesXYZ = glm::vec3(0.0f);
	glm::vec3 scale = glm::vec3(1.0f);
};

static std::string ToLower(std::string value) {
	for (char& c : value) {
		c = (char)std::tolower((unsigned char)c);
	}
	return value;
}

static std::string ResolvePath(const std::string& baseFile, const std::string& candidate) {
	namespace fs = std::filesystem;
	fs::path p(candidate);
	if (p.is_absolute()) {
		return p.lexically_normal().string();
	}
	fs::path base(baseFile);
	if (base.has_parent_path()) {
		return (base.parent_path() / p).lexically_normal().string();
	}
	return p.lexically_normal().string();
}

static bool ParseVec3Node(const YAML::Node& n, glm::vec3& out) {
	if (!n || !n.IsSequence() || n.size() != 3) {
		return false;
	}
	out.x = n[0].as<float>();
	out.y = n[1].as<float>();
	out.z = n[2].as<float>();
	return true;
}

static Transform ParseTransform(const YAML::Node& node) {
	Transform t;
	if (!node || !node.IsMap()) {
		return t;
	}
	if (node["translation"]) {
		ParseVec3Node(node["translation"], t.translation);
	}
	if (node["rotation_degrees_xyz"]) {
		ParseVec3Node(node["rotation_degrees_xyz"], t.rotationDegreesXYZ);
	}
	if (node["scale"]) {
		ParseVec3Node(node["scale"], t.scale);
	}
	return t;
}

static glm::mat4 ComposeTransform(const Transform& t) {
	glm::mat4 m(1.0f);
	m = glm::translate(m, t.translation);
	m = glm::rotate(m, glm::radians(t.rotationDegreesXYZ.x), glm::vec3(1.0f, 0.0f, 0.0f));
	m = glm::rotate(m, glm::radians(t.rotationDegreesXYZ.y), glm::vec3(0.0f, 1.0f, 0.0f));
	m = glm::rotate(m, glm::radians(t.rotationDegreesXYZ.z), glm::vec3(0.0f, 0.0f, 1.0f));
	m = glm::scale(m, t.scale);
	return m;
}

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
	(void)lineargs;
	(void)numargs;
	(void)state;
	return true;
}

bool ParseMTL_newmtl(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
	if (numargs != 2) {
		ERROR("Invalid format for newmtl arguments, must have only 1 argument - detected %d instead", (int)numargs - 1);
		return false;
	}
	state->mnames.emplace_back(lineargs[1]);
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
	snprintf(mtlpath, MAX_MTLLIB_PATH_SIZE, "%s%s", mtlloc, lineargs[1]);
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
		size_t numargs2 = ParseLineArgsOBJ(line, lineargs);
		if (numargs2 > 0 && lineargs[0][0] != '#') {
			ParseFuncMTL p = GetParserFromArgMTL(lineargs[0]);
			if (p) { if ((failure = !p(lineargs, numargs2, state))) { ERROR("%s:%d - Unable to parse this MTL field due to an error", mtlpath, (int)parser.line); break; }
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
	float u, v;
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
	(void)lineargs;
	(void)numargs;
	(void)state;
	return true;
}

bool ParseOBJ_o(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
	(void)lineargs;
	(void)numargs;
	(void)state;
	return true;
}

bool ParseOBJ_s(char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE], size_t numargs, StateOBJ* state) {
	(void)lineargs;
	(void)numargs;
	(void)state;
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

bool ConstructOBJ(const StateOBJ state, Geometry& geometry) {
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
	size_t vertices_start = geometry.vertices.size();
	size_t normals_start = geometry.normals.size();
	for (size_t i = 0; i < state.materials.size(); i++) {
		ids.push_back((uint32_t)geometry.materials.size());
		geometry.materials.push_back(state.materials[i]);
	}
	for (size_t i = 0; i < state.vertices.size(); i++)
		geometry.vertices.push_back(glm::vec4(state.vertices[i], 0));
	for (size_t i = 0; i < state.normals.size(); i++)
		geometry.normals.push_back(glm::vec4(state.normals[i], 0));
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
		if (geometry.materials[current_material].emission != glm::vec3(0.0f)) geometry.emissives.push_back(geometry.triangles.size());

		geometry.triangles.push_back(triangle);
	}
	return true;
}

void ApplyTransform(StateOBJ& state, const Transform& transform) {
	glm::mat4 model = ComposeTransform(transform);
	glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
	for (glm::vec3& v : state.vertices) {
		v = glm::vec3(model * glm::vec4(v, 1.0f));
	}
	for (glm::vec3& n : state.normals) {
		glm::vec3 transformed = normalMatrix * n;
		if (glm::length(transformed) > 0.0f) {
			n = glm::normalize(transformed);
		}
	}
}

void ApplyCamera(const YAML::Node& cameraNode) {
	if (!cameraNode || !cameraNode.IsMap()) {
		return;
	}
	camera& cam = Editor::Get()->GetRenderer().GetCamera();
	glm::vec3 v;
	if (ParseVec3Node(cameraNode["position"], v)) {
		cam.Position() = v;
	}
	if (ParseVec3Node(cameraNode["look"], v)) {
		cam.setLook(v);
	}
	if (ParseVec3Node(cameraNode["up"], v)) {
		cam.setUp(v);
	}
	if (cameraNode["fov_degrees"]) {
		cam.Fov() = cameraNode["fov_degrees"].as<float>();
	}
	if (cameraNode["orbiting"]) {
		cam.IsOrbiting() = cameraNode["orbiting"].as<bool>();
	}
	if (ParseVec3Node(cameraNode["orbit_target"], v)) {
		cam.OrbitTarget() = v;
	}
}

bool AppendEmissiveQuad(const YAML::Node& lightNode, Geometry& geometry) {
	if (!lightNode || !lightNode.IsMap()) {
		return false;
	}
	std::string type = lightNode["type"] ? lightNode["type"].as<std::string>() : "";
	if (type != "emissive_quad") {
		WARN("Unsupported light type \"%s\" in scene file. Skipping", type.c_str());
		return true;
	}
	float width = lightNode["width"] ? lightNode["width"].as<float>() : 1.0f;
	float height = lightNode["height"] ? lightNode["height"].as<float>() : 1.0f;
	glm::vec3 emission(1.0f);
	if (!ParseVec3Node(lightNode["emission"], emission)) {
		WARN("Light is missing valid emission [r,g,b]. Falling back to [1,1,1]");
	}
	Transform transform = ParseTransform(lightNode["transform"]);
	glm::mat4 model = ComposeTransform(transform);
	glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

	const size_t vStart = geometry.vertices.size();
	const size_t nStart = geometry.normals.size();
	const uint32_t matIndex = (uint32_t)geometry.materials.size();

	Material m{};
	m.emission = emission;
	m.ambient = glm::vec3(0.0f);
	m.diffuse = glm::vec3(0.0f);
	m.specular = glm::vec3(0.0f);
	m.absorbtion = glm::vec3(0.0f);
	m.dispersion = glm::vec3(0.0f);
	m.ior = 1.0f;
	m.shiny = 0.0f;
	m.model = 0;
	geometry.materials.push_back(m);

	const float hx = width * 0.5f;
	const float hy = height * 0.5f;
	const glm::vec3 localVerts[4] = {
		glm::vec3(-hx, -hy, 0.0f),
		glm::vec3( hx, -hy, 0.0f),
		glm::vec3( hx,  hy, 0.0f),
		glm::vec3(-hx,  hy, 0.0f)
	};

	for (int i = 0; i < 4; i++) {
		geometry.vertices.push_back(glm::vec4(glm::vec3(model * glm::vec4(localVerts[i], 1.0f)), 0.0f));
	}

	glm::vec3 worldNormal = glm::normalize(normalMatrix * glm::vec3(0.0f, 0.0f, 1.0f));
	for (int i = 0; i < 4; i++) {
		geometry.normals.push_back(glm::vec4(worldNormal, 0.0f));
	}

	geometry.triangles.push_back(Triangle{
		(uint32_t)vStart + 0, (uint32_t)vStart + 1, (uint32_t)vStart + 2,
		(uint32_t)nStart + 0, (uint32_t)nStart + 1, (uint32_t)nStart + 2,
		matIndex
	});
	geometry.triangles.push_back(Triangle{
		(uint32_t)vStart + 0, (uint32_t)vStart + 2, (uint32_t)vStart + 3,
		(uint32_t)nStart + 0, (uint32_t)nStart + 2, (uint32_t)nStart + 3,
		matIndex
	});
	return true;
}

bool LoadOBJFile(const std::string& filepath, const Transform& transform, Geometry& geometry) {
	SimpleFile* file = VFILE::ReadFile(filepath.c_str());
	if (!file) {
		ERROR("Unable to load invalid filepath \"%s\"", filepath.c_str());
		return false;
	}
	if (file->type != DOTOBJ) {
		ERROR("\"%s\" is not a .obj file. Unable to open it with the OBJ loader", filepath.c_str());
		VFILE::FreeFile(file);
		return false;
	}
	LineParser parser = VFILE::Parser(file);
	StateOBJ state{};
	state.filepath = filepath;
	char line[MAX_OBJ_LINE_SIZE] = { 0 };
	while (VFILE::NextLine(&parser, line, MAX_OBJ_LINE_SIZE)) {
		char lineargs[MAX_OBJ_NUM_ARGS][MAX_OBJ_ARG_SIZE] = { 0 };
		size_t numargs = ParseLineArgsOBJ(line, lineargs);
		if (numargs > 0 && lineargs[0][0] != '#') {
			ParseFuncOBJ p = GetParserFromArgOBJ(lineargs[0]);
			if (p) {
				if (!p(lineargs, numargs, &state)) {
					ERROR("%s:%d - Unable to parse this OBJ field due to an error", filepath.c_str(), (int)parser.line);
					VFILE::FreeFile(file);
					return false;
				}
			} else {
				WARN("%s:%d - Unknown OBJ property detected: \"%s\", skipping parsing this field...", filepath.c_str(), (int)parser.line, lineargs[0]);
			}
		}
	}
	ApplyTransform(state, transform);
	bool ok = ConstructOBJ(state, geometry);
	VFILE::FreeFile(file);
	return ok;
}

void RebuildDerivedGeometry(Geometry& geometry) {
	geometry.emissives.clear();
	for (size_t i = 0; i < geometry.triangles.size(); i++) {
		uint32_t m = geometry.triangles[i].material;
		if (m < geometry.materials.size()) {
			const glm::vec3 e = geometry.materials[m].emission;
			if (e.x > 0.0f || e.y > 0.0f || e.z > 0.0f) {
				geometry.emissives.push_back((uint32_t)i);
			}
		}
	}
	geometry.bvh = BVH::Create(geometry.triangles, geometry.vertices);
	geometry.bvh_size = geometry.bvh.size();
	geometry.vertices_size = geometry.vertices.size();
	geometry.normals_size = geometry.normals.size();
	geometry.triangles_size = geometry.triangles.size();
	geometry.emissives_size = geometry.emissives.size();
	geometry.materials_size = geometry.materials.size();
}

void ResetGeometry(Geometry& geometry) {
	geometry.bvh.clear();
	geometry.vertices.clear();
	geometry.normals.clear();
	geometry.triangles.clear();
	geometry.emissives.clear();
	geometry.materials.clear();
	geometry.bvh_size = 0;
	geometry.vertices_size = 0;
	geometry.normals_size = 0;
	geometry.triangles_size = 0;
	geometry.emissives_size = 0;
	geometry.materials_size = 0;
}

bool LoadYAMLScene(const std::string& filepath, Geometry& geometry) {
	YAML::Node root;
	try {
		root = YAML::LoadFile(filepath);
	} catch (const std::exception& e) {
		ERROR("Failed to parse YAML scene \"%s\": %s", filepath.c_str(), e.what());
		return false;
	}

	if (root["camera"]) {
		ApplyCamera(root["camera"]);
	}

	if (root["meshes"]) {
		if (!root["meshes"].IsSequence()) {
			ERROR("Scene YAML field \"meshes\" must be a sequence");
			return false;
		}
		for (size_t i = 0; i < root["meshes"].size(); i++) {
			const YAML::Node mesh = root["meshes"][i];
			if (!mesh["obj"]) {
				ERROR("Mesh entry %d is missing required field \"obj\"", (int)i);
				return false;
			}
			const std::string objPath = ResolvePath(filepath, mesh["obj"].as<std::string>());
			const Transform transform = ParseTransform(mesh["transform"]);
			if (!LoadOBJFile(objPath, transform, geometry)) {
				return false;
			}
		}
	}

	if (root["lights"]) {
		if (!root["lights"].IsSequence()) {
			ERROR("Scene YAML field \"lights\" must be a sequence");
			return false;
		}
		for (size_t i = 0; i < root["lights"].size(); i++) {
			if (!AppendEmissiveQuad(root["lights"][i], geometry)) {
				ERROR("Failed to append light entry %d", (int)i);
				return false;
			}
		}
	}

	RebuildDerivedGeometry(geometry);
	return true;
}

} // namespace

bool LoadScene(const std::string& filepath, Geometry& geometry) {
	const std::string ext = ToLower(std::filesystem::path(filepath).extension().string());
	ResetGeometry(geometry);

	if (ext == ".yaml" || ext == ".yml") {
		return LoadYAMLScene(filepath, geometry);
	}
	if (ext == ".obj") {
		const Transform identity;
		if (!LoadOBJFile(filepath, identity, geometry)) {
			return false;
		}
		RebuildDerivedGeometry(geometry);
		return true;
	}

	ERROR("Unsupported scene file extension for \"%s\". Expected .yaml/.yml or .obj", filepath.c_str());
	return false;
}

} // namespace VSTIR::SceneLoader

