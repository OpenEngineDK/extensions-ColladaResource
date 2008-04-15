// Collada Model resource.
// -------------------------------------------------------------------
// Copyright (C) 2007 OpenEngine.dk (See AUTHORS) 
// Modified by Anders Bach Nielsen <abachn@daimi.au.dk> - 21. Nov 2007
// 
// This program is free software; It is covered by the GNU General 
// Public License version 2 or any later version. 
// See the GNU General Public License for more details (see LICENSE). 
//--------------------------------------------------------------------

#ifndef _COLLADA_RESOURCE_H_
#define _COLLADA_RESOURCE_H_

#include <Resources/IModelResource.h>
#include <Resources/ResourcePlugin.h>
#include <Resources/ITextureResource.h>
#include <Resources/IShaderResource.h>
#include <Geometry/FaceSet.h>

#include <string>
#include <vector>

// Collada dom classes
#include <dae.h>
#include <dom/domCOLLADA.h> // include the most common dom nodes
#include <dom/domConstants.h>

namespace OpenEngine {
namespace Resources {

using namespace OpenEngine::Geometry;
using namespace std;

/**
 * Collada resource.
 *
 * @class ColladaResource ColladaResource.h "ColladaResource.h"
 */
class ColladaResource : public IModelResource {
private:
    
    struct InputMap{
        int size; // the number of floats to write(assume that all data arrays are of type float)
        int stride; // the p index must be multiplied with this number
        float* dest; // a pointer to a float array where the data has to be written
        domListOfFloats src; // the source element
    };

    // inner material structure
    class Material {
    public:
        ITextureResourcePtr texture; //!< texture resource 
        IShaderResourcePtr shader;   //!< shader resource 
        Material() {}
        ~Material() {}
    };

    string file;                      //!< collada file path
    FaceSet* faces;                   //!< the face set
    map<string, Material*> materials; //!< resources material map

    // Collada DOM pointers
    DAE *dae;

    // helper methods
    void Error(int line, string msg);

public:
    ColladaResource(string file);
    ~ColladaResource();
    void Load();
    void Unload();
    FaceSet* GetFaceSet();
};

/**
 * Collada resource plug-in.
 *
 * @class ColladaPlugin ColladaResource.h "ColladaResource.h"
 */
class ColladaPlugin : public ResourcePlugin<IModelResource> {
public:
	ColladaPlugin();
    IModelResourcePtr CreateResource(string file);
};

} // NS Resources
} // NS OpenEngine

#endif // _COLLADA_MODEL_RESOURCE_H_
