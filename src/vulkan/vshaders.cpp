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
    }
        WARN("Unable to automatically identify source references of shader variable \"%s\"", name);
    	return (VulkanBoundVariable){};
    }

    VulkanShader VSHADERS::GenerateShader(std::string filepath, std::string objpath) {
        VulkanShader shader{};
        shader.filename = objpath;
        FILE* f = fopen(filepath.c_str(), "r");
        if (f) {
            char line[512] = { 0 };
    		int linecount = 0;
    		int num_vars = 0;
            std::vector<int> indices;
            std::vector<VulkanBoundVariable> vbvs;
    		while (fgets(line, sizeof(line), f)) {
    			linecount++;
    			int linelen = strlen(line);
    			if (linelen >= 512) 
    				WARN("Abnormally long line length detected on line %d in shader %s, this may have adverse effects on shader parsing", linecount, filepath.c_str());
    			char* bindstr = strstr(line, "layout(binding");
    			if (!bindstr) bindstr = strstr(line, "layout (binding");
    			if (bindstr) {
    				int ind = 0;
    				while (bindstr[ind] != '\0') {
    					if (bindstr[ind] >= '0' && bindstr[ind] <= '9') break;
    					ind++;
    				}
    				if (bindstr[ind] != '\0') {
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
    					num_vars++;
    				} else {
    					WARN("Unable to detect a binding on line %d: %s", linecount, bindstr);
    				}
    			}
    		}
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
        } else {
            FATAL("Shader cannot load invalid file - unable to read file %s", filepath.c_str());
        }
        fclose(f);
        return shader;
    }

}
