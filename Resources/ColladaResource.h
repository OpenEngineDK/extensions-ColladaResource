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
#include <Geometry/Material.h>
#include <Math/Quaternion.h>

#include <string>
#include <vector>
#include <map>

// Collada dom classes
#include <dae.h>
#include <dom/domCOLLADA.h> // include the most common dom nodes
#include <dom/domConstants.h>
#include <dom/domCommon_color_or_texture_type.h>
#include <dom/domProfile_COMMON.h>

namespace OpenEngine {
    //forward declarations
    namespace Scene { 
        class ISceneNode; 
        class GeometryNode; 
        class TransformationNode;
    }
    namespace Geometry {
        class FaceSet;
    }

namespace Resources {

using namespace OpenEngine::Scene;
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
        int size;            //!< the number of floats to write(assume that all data arrays are of type float)
        int stride;          //!< the p index must be multiplied with this number
        float* dest;         //!< a pointer to a float array where the data has to be written
        domListOfFloats src; //!< the source element
    };
    
    // data caches
    map<string, MaterialPtr> materials;
    map<string,domCommon_newparam_type*> params;
    
    Quaternion<float> rot;
    bool yUp;

    float vertex[3], normal[3], texcoord[2], color[3]; //!< buffers for the vertex data
    vector<InputMap*>* offsetMap;

    string file;                      //!< collada file path
    TransformationNode* root;                 //!< the root node
    //    map<string, Material*> materials; //!< resources material map

    // Collada DOM pointers
    DAE *dae;

    // helper methods
    GeometryNode* LoadGeometry(domInstance_geometry* geom);
    MaterialPtr LoadMaterial(domMaterial* dm);

    void ReadImage(domImage* img, MaterialPtr m);
    void ReadNode(domNode* dNode, ISceneNode* sNode);
    void ReadEffect(domInstance_effect* eInst, MaterialPtr m);
    void ReadTriangles(domTriangles* ts, FaceSet* fs);
    void ReadColor(domCommon_color_or_texture_type_complexType* ct,
                              Vector<4,float>* dest);
    
    void ReadPhong(domProfile_COMMON::domTechnique::domPhong* phong,
                   MaterialPtr m);

    void ReadLambert(domProfile_COMMON::domTechnique::domLambert* lambert,
                   MaterialPtr m);

    void ReadBlinn(domProfile_COMMON::domTechnique::domBlinn* blinn,
                   MaterialPtr m);

    void ReadConstant(domProfile_COMMON::domTechnique::domConstant* constant,
                   MaterialPtr m);


    void ReadTexture(domCommon_color_or_texture_type_complexType::domTexture* tex, 
                     MaterialPtr m);
    void ProcessInputLocalOffset(domInputLocalOffset* input);
    void InsertInputMap(daeString semantic, domSource* src, int offset);

public:
    ColladaResource(string file);
    ~ColladaResource();
    void Load();
    void Unload();
    ISceneNode* GetSceneNode();
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
