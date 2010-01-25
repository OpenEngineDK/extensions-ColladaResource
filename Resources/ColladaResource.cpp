// Collada model resource.
// -------------------------------------------------------------------
// Copyright (C) 2007 OpenEngine.dk (See AUTHORS) 
// 
// 
// This program is free software; It is covered by the GNU General 
// Public License version 2 or any later version. 
// See the GNU General Public License for more details (see LICENSE). 
//--------------------------------------------------------------------

#include <Resources/ColladaResource.h>

#include <Resources/ITextureResource.h>
#include <Resources/ResourceManager.h>
#include <Resources/File.h>
#include <Logging/Logger.h>
#include <Utils/Convert.h>
#include <Geometry/FaceSet.h>
#include <Scene/GeometryNode.h>
#include <Scene/ISceneNode.h>
#include <Scene/TransformationNode.h>




//#include <dae/daeSIDResolver.h>

namespace OpenEngine {
namespace Resources {

using namespace OpenEngine::Logging;
using OpenEngine::Utils::Convert;
using namespace OpenEngine::Geometry;


// PLUG-IN METHODS

/**
 * Get the file extension for Collada files.
 */
ColladaPlugin::ColladaPlugin() {
    this->AddExtension("dae");
}

/**
 * Create a Collada resource.
 */
IModelResourcePtr ColladaPlugin::CreateResource(string file) {
    return IModelResourcePtr(new ColladaResource(file));
}


// RESOURCE METHODS

/**
 * Resource constructor.
 */
ColladaResource::ColladaResource(string file) : file(file), root(NULL), dae(NULL) {}

/**
 * Resource destructor.
 */
ColladaResource::~ColladaResource() {
    Unload();
}

void ColladaResource::ReadImage(domImage* img,
                                MaterialPtr m) {
    // we reset the resource path temporary to create the texture resource
    string resource_dir = File::Parent(this->file);

	if (! DirectoryManager::IsInPath(resource_dir)) {
        DirectoryManager::AppendPath(resource_dir);
    }

    domImage::domInit_from* initFrom = img->getInit_from();
    if (initFrom != NULL) {
        m->texr = ResourceManager<ITextureResource>::Create(initFrom->getValue().getOriginalURI());
    }
}

/**
 * Helper function to load a domTexture into a Material structure.
 */
void ColladaResource::ReadTexture(domCommon_color_or_texture_type_complexType::domTexture* tex,
                                  MaterialPtr m) {
    // TODO: this is pretty hacked up ugly code ... MAKE NICER!
    
    // try do evaluate the sid
    daeIDRef id(tex->getTexture());
    id.setContainer(tex);
    daeElement* elm = id.getElement();

    // if this doesn't work then look in our newparam map
    if (elm == NULL) {
        elm = params[tex->getTexture()];
        if (elm == NULL) {
            logger.warning << "Invalid texture reference: " << 
                tex->getTexture() << ". No texture Loaded." << logger.end;
            return;
        }
    }
        
    // If the texture reference points to an image node
    if (elm->getElementType() == COLLADA_TYPE::IMAGE) {
        ReadImage(dynamic_cast<domImage*>(elm),m);
        return;
    }
    
    // If the texture points to a common new param node
    if (elm->getElementType() == COLLADA_TYPE::COMMON_NEWPARAM_TYPE) {
        domFx_sampler2D_common* sampler = dynamic_cast<domCommon_newparam_type*>(elm)->getSampler2D();
        if (sampler == NULL)
            return;
        daeElement* src = params[sampler->getSource()->getValue()];
        if (src == NULL) 
            return;
        domFx_surface_common* surface = dynamic_cast<domCommon_newparam_type*>(src)->getSurface();
        if (surface == NULL || surface->getType() != FX_SURFACE_TYPE_ENUM_2D)
            return;
        if (surface->getFx_surface_init_common()->getInit_from_array().getCount() != 0) {
            daeElement* init = surface->getFx_surface_init_common()->getInit_from_array()[0]->getValue().getElement();
            if (init->getElementType() == COLLADA_TYPE::IMAGE) {
                ReadImage(dynamic_cast<domImage*>(init),m);
                return;
            }
        }
    }
}

MaterialPtr ColladaResource::LoadMaterial(domMaterial* dm) {
    // see if material has already been loaded.

    if (dm == NULL) {
        logger.warning << "No material found. Fall back to default material." << logger.end;
        return MaterialPtr(new Material());
    } 
    
    MaterialPtr m = materials[dm->getID()];
    
    // TODO: maybe we should copy the material as it is a new instance???
    if (m != NULL)
        return m;
    
    // create material with default values
    // and add it to the material map
    m = MaterialPtr(new Material());
    materials[dm->getID()] = m;
    

    // read the effect into the material.
    ReadEffect(dm->getInstance_effect(), m);

    return m;
}

void ColladaResource::ReadColor(domCommon_color_or_texture_type_complexType* ct, 
                                Vector<4,float>* dest) {
    if (ct != NULL && ct->getColor() != NULL) {
        domFx_color_common col = ct->getColor()->getValue();
        *dest = Vector<4,float>(col[0],col[1],col[2],col[3]);
    }
}

void ColladaResource::ReadPhong(domProfile_COMMON::domTechnique::domPhong* phong,
                                MaterialPtr m) {
    if (phong->getDiffuse() != NULL) {
        //check for texture in diffuse channel
        if (phong->getDiffuse()->getTexture() != NULL)
            ReadTexture(phong->getDiffuse()->getTexture(),m);
    }

    ReadColor(phong->getDiffuse(), &m->diffuse);
    ReadColor(phong->getAmbient(), &m->ambient);
    ReadColor(phong->getSpecular(), &m->specular);
    ReadColor(phong->getEmission(), &m->emission);
    
    if (phong->getShininess() != NULL) {
        if (phong->getShininess()->getFloat() != NULL) {
            // check for shininess value
            m->shininess = phong->getShininess()->getFloat()->getValue();
        }
    }
}

void ColladaResource::ReadLambert(domProfile_COMMON::domTechnique::domLambert* lambert,
                                MaterialPtr m) {
    if (lambert->getDiffuse() != NULL) {
        //check for texture in diffuse channel
        if (lambert->getDiffuse()->getTexture() != NULL)
            ReadTexture(lambert->getDiffuse()->getTexture(),m);
    }

    ReadColor(lambert->getDiffuse(), &m->diffuse);
    ReadColor(lambert->getAmbient(), &m->ambient);
    ReadColor(lambert->getEmission(), &m->emission);
}

void ColladaResource::ReadBlinn(domProfile_COMMON::domTechnique::domBlinn* blinn,
                                MaterialPtr m) {
    if (blinn->getDiffuse() != NULL) {
        //check for texture in diffuse channel
        if (blinn->getDiffuse()->getTexture() != NULL)
            ReadTexture(blinn->getDiffuse()->getTexture(),m);
    }

    ReadColor(blinn->getDiffuse(), &m->diffuse);
    ReadColor(blinn->getAmbient(), &m->ambient);
    ReadColor(blinn->getSpecular(),&m->specular);
    ReadColor(blinn->getEmission(), &m->emission);
    
    if (blinn->getShininess() != NULL) {
        if (blinn->getShininess()->getFloat() != NULL) {
            // check for shininess value
            m->shininess = blinn->getShininess()->getFloat()->getValue();
        }
    }
}

void ColladaResource::ReadConstant(domProfile_COMMON::domTechnique::domConstant* constant,
                                MaterialPtr m) {
    ReadColor(constant->getEmission(), &m->emission);
    
}

void ColladaResource::ReadEffect(domInstance_effect* eInst, MaterialPtr m) {
    // get the effect element pointed to by instance_effect
    domEffect* e = dynamic_cast<domEffect*>(eInst->getUrl().getElement().cast());
    if (e == NULL) {
        logger.warning << "Could not resolve effect uri." << logger.end;
        return;
    }
    
    domFx_profile_abstract_Array profileArr = e->getFx_profile_abstract_array();
 
    for (unsigned int i = 0; i < profileArr.getCount(); i++) {
        // only process the profile_COMMON technique
        if (profileArr[i]->getElementType() == COLLADA_TYPE::PROFILE_COMMON) {
            domProfile_COMMON* profile_common = 
                dynamic_cast<domProfile_COMMON*>(profileArr[i].cast());
            domProfile_COMMON::domTechnique* technique = 
                profile_common->getTechnique();
            
            params.clear();
            domCommon_newparam_type_Array newparam_array = profile_common->getNewparam_array();
            for (unsigned int j = 0; j < newparam_array.getCount(); j++) {
                params[newparam_array[j]->getSid()] = newparam_array[j];
            }


            domProfile_COMMON::domTechnique::domPhong* phong = technique->getPhong();
            domProfile_COMMON::domTechnique::domLambert* lambert = technique->getLambert();
            domProfile_COMMON::domTechnique::domBlinn* blinn = technique->getBlinn();
            domProfile_COMMON::domTechnique::domConstant* constant = technique->getConstant();

            if (phong != NULL) {
                ReadPhong(phong, m);
            }

            if (lambert != NULL) {
                ReadLambert(lambert, m);
            }

            if (blinn != NULL) {
                ReadBlinn(blinn, m);
            }

            if (constant != NULL) {
                ReadConstant(constant, m);
            }
        }
    }
}

/**
* Helper function to load the geometry from a given domGeometry node
*/
GeometryNode* ColladaResource::LoadGeometry(domInstance_geometry* gInst) {
    domGeometry* geom = dynamic_cast<domGeometry*>(gInst->getUrl().getElement().cast());
    if (!geom) 
        return NULL;

    FaceSet* fs = new FaceSet();
    domMesh* mesh = geom->getMesh();
    
    // Display warnings if unsupported geometry types are defined
    if (mesh->getLines_array().getCount() > 0)
        logger.warning << "Unsupported geometry types found: Lines" << logger.end;
    if (mesh->getLinestrips_array().getCount() > 0)
        logger.warning << "Unsupported geometry types found: Linestrips" << logger.end;
    if (mesh->getPolygons_array().getCount() > 0)
        logger.warning << "Unsupported geometry types found: Polygons" << logger.end;
    if (mesh->getPolylist_array().getCount() > 0)
        logger.warning << "Unsupported geometry types found: Polylist" << logger.end;
    if (mesh->getTrifans_array().getCount() > 0)
        logger.warning << "Unsupported geometry types found: Trifans" << logger.end;
    if (mesh->getTristrips_array().getCount() > 0)
        logger.warning << "Unsupported geometry types found: Tristrips" << logger.end;


    // read the triangle data into our face set.
    domTriangles_Array trianglesArr = mesh->getTriangles_array();
    for (unsigned int i = 0; i < trianglesArr.getCount(); i++) {
        ReadTriangles(trianglesArr[i],fs); 
    }   
        
    return new GeometryNode(fs);
}

void ColladaResource::ReadTriangles(domTriangles* ts, FaceSet* fs) {
    // Get the material associated with the current triangle list
    daeIDRef mRef(ts->getMaterial());
    mRef.setContainer(ts);
    MaterialPtr m =
        LoadMaterial(dynamic_cast<domMaterial*>(mRef.getElement()));
    
    // Retrieve the primitive(P) list, which is a list of indices into 
    // source elements. 
    domListOfUInts pArr = ts->getP()->getValue(); 
    int pCount = pArr.getCount();
    
    // Retrieve an array of input elements. These elements define the type of data that 
    // a certain P-index points to (vertex, normal, etc.). It also indicates which
    // source element the P-index points into. 
    domInputLocalOffset_Array inputArr = ts->getInput_array();
    int inputCount = inputArr.getCount(); // number of attributes per vertex in triangle
    
    // A map indicating where the retrieved vertex data should be copied to
    // depending on the input offset (we assume that the highest possible
    // offset is the number of input elements).
    offsetMap = new vector<InputMap*>[inputCount];
    
    // fill out the offsetMap and find the max offset number            
    int maxOffset = 0;
    for (int input = 0; input < inputCount; input++) {
        int offset = inputArr[input]->getOffset();
        if (offset > maxOffset)
            maxOffset = offset;
        
        ProcessInputLocalOffset(inputArr[input].cast());
    }
    
    // Start iterating through each p-index
    int currentP = 0;
    int currentVertex = 0;
    int currentOffset = 0;
    int currentFace  = 0;
    Vector<3,float> vertices[3];
    Vector<3,float> normals[3];
    Vector<2,float> texcoords[3];
    Vector<4,float> colors[3];
    
    while (currentP < pCount) {
        int p = pArr[currentP];
            
        for (vector<InputMap*>::iterator itr = offsetMap[currentOffset].begin();
             itr != offsetMap[currentOffset].end();
             itr++) {
            InputMap* im = *itr;
            if (im->dest != NULL) {
                // for each p index we read "size" values beginning from "p*stride"
                for (int s = 0; s < im->size; s++) {
                        im->dest[s] = im->src[p*im->stride+s];
                }
            }
        }
        
        currentOffset++;
        if (currentOffset > maxOffset) {
            currentOffset = 0;
            
            vertices[currentVertex] = Vector<3,float>(vertex[0],vertex[1],vertex[2]);
            normals[currentVertex] = Vector<3,float>(normal[0],normal[1],normal[2]);
            texcoords[currentVertex] = Vector<2,float>(texcoord[0],texcoord[1]);
            colors[currentVertex] = Vector<4,float>(color[0],color[1],color[2],1.0);

            // here we rotate the coordinates if y is not the up axis
            // TODO: this could probably be done faster by a coordinate switch
            if (!yUp) {
                vertices[currentVertex] = rot.RotateVector(vertices[currentVertex]);
                normals[currentVertex] = rot.RotateVector(normals[currentVertex]);
            }

            currentVertex++;
            
            
            if (currentVertex > 2) {
                currentVertex = 0;
                try {
                    FacePtr face = FacePtr(new Face(vertices[0], vertices[1],vertices[2],
                                                    normals[0], normals[1], normals[2]));
                    face->colr[0] = colors[0];
                    face->colr[1] = colors[1];
                    face->colr[2] = colors[2];
                    face->texc[0] = texcoords[0];
                    face->texc[1] = texcoords[1];
                    face->texc[2] = texcoords[2];
                    
                    face->mat = m;
                    fs->Add(face);
                }
                catch (Exception e) {
                    logger.warning << "Face caused an exception: " << e.what() << logger.end;
                } 
                currentFace++;
            }
        }
        currentP++;
    }
    
    // delete all the input maps
    for (int i = 0; i < inputCount; i++) {
        for (vector<InputMap*>::iterator itr = offsetMap[i].begin();
             itr != offsetMap[i].end();
             itr++) {
            delete *itr;
        }
    }
    delete[] offsetMap;
}
    
/**
* Helper function to create and insert an InputMap.
* This has the side effect of inserting an inputMap into
* the offsetMap array field on the index corresponding to offset.
* Make sure that the size of the array is at least as large as the offset value.
*/
void ColladaResource::InsertInputMap(daeString semantic, domSource* src, int offset) {
    InputMap* im = new InputMap();
    im->dest = NULL;
 
    if (strcmp(semantic, COMMON_PROFILE_INPUT_POSITION) == 0) {
        im->dest = vertex;
        im->size = 3;
    }
    
    else if (strcmp(semantic,COMMON_PROFILE_INPUT_NORMAL) == 0) {
        im->dest = normal;
        im->size = 3;
    }
    
    else if (strcmp(semantic,COMMON_PROFILE_INPUT_TEXCOORD) == 0) {
        im->dest = texcoord;
        im->size = 2;
    }
    else if (strcmp(semantic,COMMON_PROFILE_INPUT_COLOR) == 0) {
        im->dest = color;
        im->size = 3;
    }
    else {
        logger.warning << "Ignoring unsupported input type: " << semantic << logger.end;
        delete im;
        return;
    }
        
    
    if (src->getFloat_array() == NULL) {
        logger.warning << "No float array present, we only support vertex data in float arrays" << logger.end;
    }

    im->src = src->getFloat_array()->getValue(); // assume that all source elements contain float arrays

    if (src->getTechnique_common() == NULL) {
        // TODO: find out what the default action is when no accessor is found
        im->stride = 1; 
        im->size = 0;
        logger.warning << "Found source without accessor." << logger.end;
    } else {
        im->stride = src->getTechnique_common()->getAccessor()->getStride();
    }
    
    offsetMap[offset].push_back(im);
}

/**
* Helper function to process an input local offset element.
* Main functionality is to handle the case where the semantic
* of the input element is VERTEX which will yield a new subtree
* of input elements.
*/
void ColladaResource::ProcessInputLocalOffset(domInputLocalOffset* input) {
    
    // special case if the semantic is INPUT_VERTEX
    if (strcmp(input->getSemantic(),COMMON_PROFILE_INPUT_VERTEX) == 0) {
        domVertices* v = dynamic_cast<domVertices*>(input->getSource().getElement().cast());
        domInputLocal_Array inputArr = v->getInput_array();
        
        for (unsigned int i = 0; i < inputArr.getCount(); i++) {
            InsertInputMap(inputArr[i]->getSemantic(), 
                           dynamic_cast<domSource*>(inputArr[i]->getSource().getElement().cast()), 
                           input->getOffset());
        }
        return;
    }
    
    InsertInputMap(input->getSemantic(), 
                   dynamic_cast<domSource*>(input->getSource().getElement().cast()),
                   input->getOffset());
}

/**
 * Load a Collada dae scene file.
 *
 * This method parses the file given to the constructor and populates a
 * scene graph with the data from the file that can be retrieved with
 * GetSceneNode().
 *
 * @see Scene::ISceneNode
 */
void ColladaResource::Load() {
    
    // check if we have loaded the resource
    if (root != NULL) return;
  

    //initialize the collada database
    dae = new DAE();

    int err = dae->load(file.data());
    if (err != DAE_OK)
        throw Exception("Error opening Collada file: " + file);
  
    // find the <scene> element, at most one element can exist
    if (dae->getDatabase()->getElementCount(NULL,COLLADA_ELEMENT_SCENE,NULL) == 0)
        throw Exception("No scene element defined in collada file");
    
    domCOLLADA::domScene* scene;
    
    err = dae->getDatabase()->getElement((daeElement**)&scene, 
                                         0, 
                                         NULL, 
                                         COLLADA_ELEMENT_SCENE, 
                                         NULL);
    
    if (err != DAE_OK)
        throw Exception("Error retrieving scene element.");
    
    // find the <instance_visual_scene> element
    // at most one element can exist
    if (scene->getInstance_visual_scene() == NULL)
        throw Exception("No visual scene instance defined.");

    root = new TransformationNode();

    // default axis settings, Y-axis is up!
    yUp = true;
    domCOLLADA* dRoot = dae->getDom(file.data());
    if (dRoot->getAsset()->getUp_axis()->getValue() == UPAXISTYPE_Z_UP) {
        logger.info << "rotating" << logger.end;
        yUp = false;
        rot = Quaternion<float>(-PI*0.5,Vector<3,float>(1.0,0.0,0.0));
        //root->SetRotation(q);
    }
    

    // process all <node> elements in the visual scene
    domVisual_scene* vs = 
        dynamic_cast<domVisual_scene*>(scene->getInstance_visual_scene()->getUrl().getElement().cast());
    domNode_Array nodeArr = vs->getNode_array();

    // recursively process each node element
    for (unsigned int n = 0; n < nodeArr.getCount(); n++) {
        ReadNode(nodeArr[n], root);
    }
}

// Helper method to recursively process a domNode in order 
// to fill out the scene graph with transformation and geometry nodes.
void ColladaResource::ReadNode(domNode* dn, ISceneNode* sn) {
    ISceneNode* node = sn;
    TransformationNode* tn;

    daeTArray<daeSmartRef<daeElement> > elms;
    dn->getChildren(elms);

    // process transformation info
    // TODO: tidy up a bit and ensure that the transformation nodes are correct!
    for (unsigned int i = 0; i < elms.getCount(); i++) {
        if (elms[i]->getElementType() == COLLADA_TYPE::LOOKAT) {
            logger.warning << "ColladaResource: Look At transformation not supported." 
                           << logger.end;
            continue;
        }

        // TODO: this matrix is most likely initialized incorrectly.
        // funny behaviour observed.
        if (elms[i]->getElementType() == COLLADA_TYPE::MATRIX) {
            tn = new TransformationNode();
            domFloat4x4 m = dynamic_cast<domMatrix*>(elms[i].cast())->getValue();
            
            tn->SetRotation(Quaternion<float>(Matrix<3,3,float>(m[0],m[1],m[2],
                                                                m[4],m[5],m[6],
                                                                m[8],m[9],m[10])));
            
            tn->SetPosition(Vector<3,float>(m[3],m[7],m[11]));
            
            tn->SetScale(Matrix<4,4,float>(1,0,0,0,
                                           0,1,0,0,
                                           0,0,1,0,
                                           m[12],m[13],m[14],1));
            node->AddNode(tn);
            node = tn;
            continue;
        }
        
        if (elms[i]->getElementType() == COLLADA_TYPE::ROTATE) {
            domFloat4 rot = dynamic_cast<domRotate*>(elms[i].cast())->getValue();
            Quaternion<float> q(rot[3], Vector<3,float>(rot[0],
                                                        rot[1],
                                                        rot[2]));
            tn = new TransformationNode();
            tn->SetRotation(q);
            node->AddNode(tn);
            node = tn;
            continue;
        }
        
        if (elms[i]->getElementType() == COLLADA_TYPE::SCALE) {
            domFloat3 scale = dynamic_cast<domScale*>(elms[i].cast())->getValue();
            
            tn = new TransformationNode();
            tn->Scale(scale[0],scale[1],scale[2]);

            node->AddNode(tn);
            node = tn;
            continue;
        }
        
        if (elms[i]->getElementType() == COLLADA_TYPE::SKEW) {
            logger.warning << "ColladaResource: Skew transformation not supported"
                           << logger.end;
            
            continue;
        }
        
        if (elms[i]->getElementType() == COLLADA_TYPE::TRANSLATE) {
            domFloat3 trans = dynamic_cast<domTranslate*>(elms[i].cast())->getValue();
            
            tn = new TransformationNode();
            tn->Move(trans[0], trans[1], trans[2]);
            node->AddNode(tn);
            node = tn;
        }
    }
    
    // process each <instance_geometry> element
    domInstance_geometry_Array geomArr = dn->getInstance_geometry_array();
    for (unsigned int g = 0; g < geomArr.getCount(); g++) {
        GeometryNode* gn = 
            LoadGeometry(geomArr[g]);
        if (!gn) 
            logger.warning << "Invalid geometry url." << logger.end;
        else
            node->AddNode(gn);
    }
    
    domInstance_node_Array nodeArr = dn->getInstance_node_array();
    for (unsigned int n = 0; n < nodeArr.getCount(); n++) {
        ReadNode(dynamic_cast<domNode*>(nodeArr[n]->getUrl().getElement().cast()), node);
    }
}

/**
 * Unload the resource.
 * Resets the root node. Does not delete the scene graph.
 */
void ColladaResource::Unload() {
    root = NULL;
    if (dae != NULL) {
        delete dae;
        dae = NULL;
    }
}

/**
 * Get the scene graph for the loaded collada data.
 *
 * @return Root node of the scene graph
 */
ISceneNode* ColladaResource::GetSceneNode() {
    return root;
}

} // NS Resources
} // NS OpenEngine
