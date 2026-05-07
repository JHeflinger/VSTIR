#include "vshaders.h"
#include "util/log.h"
#include "core/get.h"
#include <cstring>

namespace VSTIR {

    char* last_relevant_word(char* str, int len) {
	    for (int i = len - 1; i >= 0; i--) {
    		if (str[i] == ' ') {
    			if (!((str[i + 1] >= 'A' && str[i + 1] <= 'Z') ||
    				(str[i + 1] >= 'a' && str[i + 1] <= 'z'))) {
    				continue;
    			} else {
    				return str + i + 1;
    			}
    		}
    	}
    	return str;
    }

    bool is_alphanumeric(char c) {
    	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    VulkanBoundVariable get_bound_variable(const char* name) {
    	if (strcmp(name, "outputImage") == 0) {
    		return (VulkanBoundVariable) {
    			STORAGE_IMAGE,
    			(SchrodingRef) {
    				true,
    				&(_context.Target().view)
    			},
    			(SchrodingSize) { (SchrodingRef) { 0 }, 0, 0 }
    		};
        } else if (strcmp(name, "rawImage") == 0) {
    		return (VulkanBoundVariable) {
    			STORAGE_IMAGE,
    			(SchrodingRef) {
    				true,
    				&(_context.RawTarget().view)
    			},
    			(SchrodingSize) { (SchrodingRef) { 0 }, 0, 0 }
    		};
        } else if (strcmp(name, "restirImage") == 0) {
    		return (VulkanBoundVariable) {
    			STORAGE_IMAGE,
    			(SchrodingRef) {
    				true,
    				&(_context.ReSTIRTarget().view)
    			},
    			(SchrodingSize) { (SchrodingRef) { 0 }, 0, 0 }
    		};
        } else if (strcmp(name, "UniformBufferObject") == 0) {
    		return (VulkanBoundVariable) {
    			UNIFORM_BUFFER,
    			(SchrodingRef) {
    				true,
    				&(_data.UBOs().object.buffer)
    			},
    			(SchrodingSize) {
    				(SchrodingRef) {
    					false,
    					(void*)1
    				}, 0.0f,
    				sizeof(UniformBufferObject)
    			},
    		};
    	} else if (strcmp(name, "TriangleSSBOIn") == 0) {
    		return (VulkanBoundVariable) {
    			STORAGE_BUFFER,
    			(SchrodingRef) {
    				true,
    				&(_core.Geometry().triangles.buffer)
    			},
    			(SchrodingSize) {
    				(SchrodingRef) {
    					true,
    					&(_renderer.GetGeometry().triangles_size)
    				}, 0.0f,
    				sizeof(Triangle)
    			}
    		};
    	} else if (strcmp(name, "EmissivesSSBOIn") == 0) {
    		return (VulkanBoundVariable) {
    			STORAGE_BUFFER,
    			(SchrodingRef) {
    				true,
    				&(_core.Geometry().emissives.buffer)
    			},
    			(SchrodingSize) {
    				(SchrodingRef) {
    					true,
    					&(_renderer.GetGeometry().emissives_size)
    				}, 0.0f,
    				sizeof(uint32_t)
    			}
    		};
    	} else if (strcmp(name, "VertexSSBOIn") == 0) {
    		return (VulkanBoundVariable) {
    			STORAGE_BUFFER,
    			(SchrodingRef) {
    				true,
    				&(_core.Geometry().vertices.buffer)
    			},
    			(SchrodingSize) {
    				(SchrodingRef) {
    					true,
    					&(_renderer.GetGeometry().vertices_size)
    				}, 0.0f,
    				sizeof(glm::vec4)
    			}
    		};
    	} else if (strcmp(name, "NormalsSSBOIn") == 0) {
    		return (VulkanBoundVariable) {
    			STORAGE_BUFFER,
    			(SchrodingRef) {
    				true,
    				&(_core.Geometry().normals.buffer)
    			},
    			(SchrodingSize) {
    				(SchrodingRef) {
    					true,
    					&(_renderer.GetGeometry().normals_size)
    				}, 0.0f,
    				sizeof(glm::vec4)
    			}
    		};
    	} else if (strcmp(name, "MaterialsSSBOIn") == 0) {
    		return (VulkanBoundVariable) {
    			STORAGE_BUFFER,
    			(SchrodingRef) {
    				true,
    				&(_core.Geometry().materials.buffer)
    			},
    			(SchrodingSize) {
    				(SchrodingRef) {
    					true,
    					&(_renderer.GetGeometry().materials_size)
    				}, 0.0f,
    				sizeof(Material)
    			}
    		};
    	} else if (strcmp(name, "BVHNodeSSBOIn") == 0) {
    		return (VulkanBoundVariable) {
    			STORAGE_BUFFER,
    			(SchrodingRef) {
    				true,
    				&(_core.Geometry().bvh.buffer)
    			},
    			(SchrodingSize) {
    				(SchrodingRef) {
    					true,
    					&(_renderer.GetGeometry().bvh_size)
    				}, 0.0f,
    				sizeof(NodeBVH)
    			}
    		};
	} else if (strcmp(name, "RayGeneratorSSBOIn") == 0) {
		return (VulkanBoundVariable) {
			STORAGE_BUFFER,
			(SchrodingRef) {
				true,
				&(_data.SSBO().buffer)
			},
			(SchrodingSize) {
				(SchrodingRef) {
					true,
					&(_renderer.GetGeometry().raygen_size)
				}, 0.0f,
				sizeof(RayGenerator)
			}
	    };
	} else if (strcmp(name, "PreviousRayGeneratorSSBOIn") == 0) {
		return (VulkanBoundVariable) {
			STORAGE_BUFFER,
			(SchrodingRef) {
				true,
				&(_data.PreviousSSBO().buffer)
			},
			(SchrodingSize) {
				(SchrodingRef) {
					true,
					&(_renderer.GetGeometry().raygen_size)
				}, 0.0f,
				sizeof(RayGenerator)
			}
	    };
    }
        WARN("Unable to automatically identify source references of shader variable \"%s\"", name);
    	return (VulkanBoundVariable){};
    }

    static std::string ShaderDirectory(const std::string& filepath) {
        size_t slash = filepath.find_last_of("/\\");
        if (slash == std::string::npos) return std::string();
        return filepath.substr(0, slash + 1);
    }

    static bool TryParseInclude(const char* line, std::string* includePath) {
        const char* include = strstr(line, "#include");
        if (!include) return false;
        const char* firstQuote = strchr(include, '"');
        if (!firstQuote) return false;
        const char* secondQuote = strchr(firstQuote + 1, '"');
        if (!secondQuote) return false;
        includePath->assign(firstQuote + 1, secondQuote - firstQuote - 1);
        return true;
    }

    static void ScanShaderBindings(
        const std::string& filepath,
        const std::string& includeRoot,
        std::vector<int>& indices,
        std::vector<VulkanBoundVariable>& vbvs,
        int& numVars) {
        FILE* f = fopen(filepath.c_str(), "r");
        if (!f) {
            FATAL("Shader cannot load invalid file - unable to read file %s", filepath.c_str());
        }
        char line[512] = { 0 };
        int linecount = 0;
        while (fgets(line, sizeof(line), f)) {
            linecount++;
            int linelen = strlen(line);
            if (linelen >= 512) {
                WARN("Abnormally long line length detected on line %d in shader %s, this may have adverse effects on shader parsing", linecount, filepath.c_str());
            }

            std::string includePath;
            if (TryParseInclude(line, &includePath)) {
                ScanShaderBindings(includeRoot + includePath, includeRoot, indices, vbvs, numVars);
                continue;
            }

            char* bindstr = strstr(line, "layout(binding");
            if (!bindstr) bindstr = strstr(line, "layout (binding");
            if (!bindstr) continue;

            int ind = 0;
            while (bindstr[ind] != '\0') {
                if (bindstr[ind] >= '0' && bindstr[ind] <= '9') break;
                ind++;
            }
            if (bindstr[ind] == '\0') {
                WARN("Unable to detect a binding on line %d: %s", linecount, bindstr);
                continue;
            }

            char numbuff[64] = { 0 };
            int buffind = 0;
            while (bindstr[ind] >= '0' && bindstr[ind] <= '9') {
                numbuff[buffind] = bindstr[ind];
                buffind++;
                ind++;
            }
            indices.push_back(atoi(numbuff));

            char* identifier = last_relevant_word(line, linelen);
            ind = 0;
            while (identifier[ind] != '\0') {
                if (!is_alphanumeric(identifier[ind])) identifier[ind] = '\0';
                ind++;
            }
            vbvs.push_back(get_bound_variable(identifier));
            numVars++;
        }
        fclose(f);
    }

    VulkanShader VSHADERS::GenerateShader(std::string filepath, std::string objpath) {
        VulkanShader shader{};
        shader.filename = objpath;

        int num_vars = 0;
        std::vector<int> indices;
        std::vector<VulkanBoundVariable> vbvs;
        ScanShaderBindings(filepath, ShaderDirectory(filepath), indices, vbvs, num_vars);

        for (int i = 0; i < num_vars; i++) {
            size_t index = 0;
            bool found = false;
            for (size_t k = 0; k < indices.size(); k++) {
                if (indices[k] == i) {
                    index = k;
                    found = true;
                    break;
                }
            }
            if (!found) FATAL("Shader \"%s\" bind group is missing index %d", filepath.c_str(), i);
            shader.variables.push_back(vbvs[index]);
        }
        return shader;
    }

}
