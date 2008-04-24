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

#include <Resources/ResourceManager.h>
#include <Resources/File.h>
#include <Logging/Logger.h>
#include <Utils/Convert.h>
#include <Geometry/FaceSet.h>
#include <Scene/GeometryNode.h>
#include <Scene/ISceneNode.h>
#include <Scene/TransformationNode.h>
#include <Math/Quaternion.h>


#include <dom/domProfile_COMMON.h>
#include <dae/daeSIDResolver.h>

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

/**
 * Helper function to load a domTexture into a Material structure.
 */
void ColladaResource::LoadTexture(domCommon_color_or_texture_type_complexType::domTexture* tex, Material* m) {
    daeElement* elm = daeSIDResolver(tex,tex->getTexture()).getElement();
    if (elm == NULL) {
        logger.warning << "Invalid texture reference: " << tex->getTexture() << ". No texture Loaded." << logger.end;
        return;
    }
     
    // If the texture reference points to an image node
    if (elm->getElementType() == COLLADA_TYPE::IMAGE) {
        domImage* img = dynamic_cast<domImage*>(elm);
        domImage::domInit_from* initFrom =  img->getInit_from();
        if (initFrom != NULL) {
            m->texture = ResourceManager<ITextureResource>::Create(string(initFrom->getValue().getFile()));
        }
    }
}

ColladaResource::Material* ColladaResource::LoadMaterial(domMaterial* material) {
    Material* res = new Material();
    
    // get the effect element pointed to by instance_effect
    domEffect* e = dynamic_cast<domEffect*>(material->getInstance_effect()->getUrl().getElement().cast());
    domFx_profile_abstract_Array profileArr = e->getFx_profile_abstract_array();
    
    for (unsigned int p = 0; p < profileArr.getCount(); p++) {
        // only process the profile_COMMON technique
        if (profileArr[p]->getElementType() == COLLADA_TYPE::PROFILE_COMMON) {
            
            domProfile_COMMON* profile_common = dynamic_cast<domProfile_COMMON*>(profileArr[p].cast());
            domProfile_COMMON::domTechnique* technique = profile_common->getTechnique();

            // ignore fixed function shader type and simply load texture from 
            // the diffuse channel.
            

            // see if the fixed function shader technique is phong
            domProfile_COMMON::domTechnique::domPhong* phong = technique->getPhong();
            if (phong != NULL) {
                if (phong->getDiffuse().cast() != NULL) {
                    LoadTexture(phong->getDiffuse()->getTexture(), res);
                }
                return res;
            }

            // see if the fixed function shader technique is lambert
            domProfile_COMMON::domTechnique::domLambert* lambert = technique->getLambert();
            if (lambert != NULL) {
                if (lambert->getDiffuse().cast() != NULL) {
                    LoadTexture(lambert->getDiffuse()->getTexture(), res);
                }
                return res;
            }
                        
            // see if the fixed function shader technique is blinn
            domProfile_COMMON::domTechnique::domBlinn* blinn = technique->getBlinn();
            if (blinn != NULL) {
                if (blinn->getDiffuse().cast() != NULL) {
                    LoadTexture(blinn->getDiffuse()->getTexture(), res);
                }
                return res;
            }
        }
    }
    return res;
}


/**
* Helper function to load the geometry from a given domGeometry node
*/
GeometryNode* ColladaResource::LoadGeometry(domGeometry* geom) {

    FaceSet* faces = new FaceSet();
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


    // Retrieve an array of domTriangles nodes contained in the mesh 
    domTriangles_Array trianglesArr = mesh->getTriangles_array();
    
    for (unsigned int t = 0; t < trianglesArr.getCount(); t++) {

        domTriangles* triangles = trianglesArr[t]; 
            
        // Get the material associated with the current triangle list
        Material* m;
        daeElement* elm = daeSIDResolver(triangles,triangles->getMaterial()).getElement();
        if (elm != NULL) {
            m = LoadMaterial(dynamic_cast<domMaterial*>(elm));
        }
        else {
            // the default material
            m = new Material();
        }

        // Retrieve the primitive(P) list, which is a list of indices into 
        // source elements. 
        domListOfUInts pArr = triangles->getP()->getValue(); 
        int pCount = pArr.getCount();

        // Retrieve an array of input elements. These elements define the type of data that 
        // a certain P-index points to (vertex, normal, etc.). It also indicates which
        // source element the P-index points into. 
        domInputLocalOffset_Array inputArr = triangles->getInput_array();
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
                currentVertex++;

                if (currentVertex > 2) {
                    currentVertex = 0;
                    try {
                        FacePtr face = FacePtr(new Face(vertices[0], vertices[1],vertices[2],
                                                        normals[0], normals[1], normals[2]));
                        face->colr[0] = Vector<4,float>(0.0,1.0,0.0,1.0);
                        face->colr[1] = Vector<4,float>(0.0,1.0,0.0,1.0);
                        face->colr[2] = Vector<4,float>(0.0,1.0,0.0,1.0);
                        face->texc[0] = texcoords[0];
                        face->texc[1] = texcoords[1];
                        face->texc[2] = texcoords[2];

                        face->texr = m->texture;
                        faces->Add(face);
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
        delete m;
    }
    
    return new GeometryNode(faces);
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
    else {
        logger.warning << "Ignoring unsupported input type: " << semantic << logger.end;
        delete im;
        return;
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
    if (err != DAE_OK) {
        logger.warning << "Error opening Collada file: " << file << logger.end;
        return;
    }
  

    // find the <scene> element 
    // at most one element can exist 
    if (dae->getDatabase()->getElementCount(NULL,COLLADA_ELEMENT_SCENE,NULL) == 0) {
        logger.info << "No scene element defined in collada file" << logger.end;
        return;
    }
    
    domCOLLADA::domScene* scene;
    
    err = dae->getDatabase()->getElement((daeElement**)&scene, 
                                         0, 
                                         NULL, 
                                         COLLADA_ELEMENT_SCENE, 
                                         NULL);
    
    if (err != DAE_OK) {
        logger.warning << "Error retrieving scene element" << logger.end;
        return;
    }
   
    
    // find the <instance_visual_scene> element
    // at most one element can exist
    if (scene->getInstance_visual_scene() == NULL) {
        logger.warning << "No visual scene instance defined" << logger.end;
        return;
    }

    root = new SceneNode();

    // process all <node> elements in the visual scene
    domNode_Array nodeArr = dynamic_cast<domVisual_scene*>(scene->getInstance_visual_scene()->getUrl().
                                                           getElement().cast())->getNode_array();

    // recursively process each node element
    for (unsigned int n = 0; n < nodeArr.getCount(); n++) {
        domNode* dNode = nodeArr[n];
        ProcessDOMNode(dNode, root);
    }
}

// Helper method to recursively process a domNode in order 
// to fill out the scene graph with transformation and geometry nodes.
void ColladaResource::ProcessDOMNode(domNode* dNode, ISceneNode* sNode) {
    ISceneNode* currNode = sNode;

    daeTArray<daeSmartRef<daeElement> > elms;
    dNode->getChildren(elms);

    //process transformation info
    //TODO: tidy up a bit and ensure that the transformation nodes are correct!
    for (unsigned int i = 0; i < elms.getCount(); i++) {
        if (elms[i]->getElementType() == COLLADA_TYPE::LOOKAT) {
            logger.warning << "ColladaResource: Look At transformation not supported." 
                           << logger.end;
            continue;
        }
        
        if (elms[i]->getElementType() == COLLADA_TYPE::MATRIX) {
            TransformationNode* t = new TransformationNode();
            domFloat4x4 m = dynamic_cast<domMatrix*>(elms[i].cast())->getValue();
            t->SetRotation(Quaternion<float>(Matrix<3,3,float>(m[0],m[1],m[2],
                                                               m[4],m[5],m[6],
                                                               m[8],m[9],m[10])));
            
            t->SetPosition(Vector<3,float>(m[3],m[7],m[11]));
            
            t->SetScale(Matrix<4,4,float>(1,0,0,0,
                                          0,1,0,0,
                                          0,0,1,0,
                                          m[12],m[13],m[14],1));
            currNode->AddNode(t);
            currNode = t;
            //logger.info << "found matrix" << logger.end;
            continue;
        }
        
        if (elms[i]->getElementType() == COLLADA_TYPE::ROTATE) {
            domFloat4 rotation = dynamic_cast<domRotate*>(elms[i].cast())->getValue();
            Quaternion<float> q(rotation[3], Vector<3,float>(rotation[0],
                                                             rotation[1],
                                                             rotation[2]));
            TransformationNode* t = new TransformationNode();
            t->SetRotation(q);
            currNode->AddNode(t);
            currNode = t;
            //logger.info << "found rotate" << logger.end;
            continue;
        }
        
        if (elms[i]->getElementType() == COLLADA_TYPE::SCALE) {
            domFloat3 scale = dynamic_cast<domScale*>(elms[i].cast())->getValue();
            
            TransformationNode* t = new TransformationNode();
            t->SetScale(Matrix<4,4,float>(1,0,0,0,
                                          0,1,0,0,
                                          0,0,1,0,
                                          scale[0], scale[1],scale[2],1));
            currNode->AddNode(t);
            currNode = t;
            //logger.info << "found scale" << logger.end;
            continue;
        }
        
        if (elms[i]->getElementType() == COLLADA_TYPE::SKEW) {
            logger.warning << "ColladaResource: Skew transformation not supported"
                           << logger.end;
            
            continue;
        }
        
        if (elms[i]->getElementType() == COLLADA_TYPE::TRANSLATE) {
            domFloat3 trans = dynamic_cast<domTranslate*>(elms[i].cast())->getValue();
            
            TransformationNode* t = new TransformationNode();
            t->Move(trans[0], trans[1], trans[2]);
            currNode->AddNode(t);
            currNode = t;
            //logger.info << "found translate" << logger.end;
        }
    }
    
    // process each <instance_geometry> element
    domInstance_geometry_Array geomArr = dNode->getInstance_geometry_array();
    for (unsigned int g = 0; g < geomArr.getCount(); g++) {
        currNode->AddNode(LoadGeometry(dynamic_cast<domGeometry*>(geomArr[g]->getUrl().getElement().cast())));
    }

    domInstance_node_Array nodeArr = dNode->getInstance_node_array();
    for (unsigned int n = 0; n < nodeArr.getCount(); n++) {
        ProcessDOMNode(dynamic_cast<domNode*>(nodeArr[n]->getUrl().getElement().cast()), currNode);
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
