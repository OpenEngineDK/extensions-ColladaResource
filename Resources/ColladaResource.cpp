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
ColladaResource::ColladaResource(string file) : file(file), node(NULL), dae(NULL) {}

/**
 * Resource destructor.
 */
ColladaResource::~ColladaResource() {
    Unload();
}

/**
 * Helper function to print out errors in the OBJ files.
 */
void ColladaResource::Error(int line, string msg) {
    logger.warning << file << " line[" << line << "] " << msg << "." << logger.end;
}

/**
* Helper function to load the geometry from a given domGeometry node
*/
GeometryNode* ColladaResource::LoadGeometry(domGeometry* geom) {

    float vertex[3], normal[3];
    FaceSet* faces = new FaceSet();
    domMesh* mesh = geom->getMesh();
    
    // Retrieve an array of domTriangles nodes contained in the mesh 
    domTriangles_Array triangles_arr = mesh->getTriangles_array();
    int triListCount = triangles_arr.getCount(); // number of triangle lists
    
    //logger.info << "Found " << triListCount << " triangle lists." << logger.end;
       
    // for each domTriangles node 
    for (int triList = 0; triList < triListCount; triList++) {

        domTriangles* triangles = triangles_arr[triList]; 
        //int triCount = triangles->getCount(); // number of triangles defined
            
        // Retrieve the primitive(P) list, which is a list of indices into 
        // source elements. 
        domListOfUInts p_arr = triangles->getP()->getValue(); 
        int pCount = p_arr.getCount();

        // Retrieve an array of input elements. These elements define the type of data that 
        // a certain P-index points to (vertex, normal, etc.). It also indicates which
        // source element the P-index points into. 
        domInputLocalOffset_Array input_arr = triangles->getInput_array();
        int inputCount = input_arr.getCount(); // number of attributes per vertex in triangle
            
        // A map indicating where the retrieved vertex data should be copied to
        // depending on the input offset (we assume that the highest possible
        // offset is the number input elements).
        vector<InputMap>* offsetMap = new vector<InputMap>[inputCount];
            
        // fill out the offsetMap and find the max offset number            
        int maxOffset = 0;
        for (int input = 0; input < inputCount; input++) {
            InputMap im;
            im.dest = NULL;

            domInputLocalOffset* domInput = input_arr[input];
                
            if (strcmp(domInput->getSemantic(),COMMON_PROFILE_INPUT_VERTEX) == 0) {
                im.dest = vertex;
                im.size = 3;
            }
            else if (strcmp(domInput->getSemantic(),COMMON_PROFILE_INPUT_NORMAL) == 0) {
                im.dest = normal;
                im.size = 3;
            }
                
            daeElement* elm = domInput->getSource().getElement();
                  
            if (elm->getElementType() == COLLADA_TYPE::VERTICES) {
                domVertices* v = (domVertices*)elm;
                domInputLocal_Array ia = v->getInput_array();
                for (unsigned int l = 0; l < ia.getCount(); l++) {
                    if (strcmp(ia[l]->getSemantic(), COMMON_PROFILE_INPUT_POSITION) == 0) {
                        elm = ia[l]->getSource().getElement();
                        break;
                    }
                }
            }
                
            domSource* src  = (domSource*)elm;
                
            im.src = src->getFloat_array()->getValue();

            if (src->getTechnique_common() == NULL) {
                im.stride = 1;
                im.size = 0;
                logger.warning << "Found source without accessor!" << logger.end;
            } else {
                im.stride = src->getTechnique_common()->getAccessor()->getStride();
            }
                
            int offset = domInput->getOffset();
            if (offset > maxOffset)
                maxOffset = offset;
                
            offsetMap[offset].push_back(im);
        }

        // Start iterating through each p-index
        int currentP = 0;
        int currentVertex = 0;
        int currentOffset = 0;
        int currentFace  = 0;
        Vector<3,float> vertices[3];
        Vector<3,float> normals[3];
            
        while (currentP < pCount) {
            int p = p_arr[currentP];
            for (vector<InputMap>::iterator itr = offsetMap[currentOffset].begin();
                 itr != offsetMap[currentOffset].end();
                 itr++) {
                InputMap im = *itr;
                if (im.dest != NULL) {
                    // for each p index we read "size" values beginning from "p*stride"
                    for (int s = 0; s < im.size; s++) {
                        im.dest[s] = im.src[p*im.stride+s];
                    }
                }
            }

            currentOffset++;
            if (currentOffset > maxOffset) {
                currentOffset = 0;
                    
                vertices[currentVertex] = Vector<3,float>(vertex[0],vertex[1],vertex[2]);
                normals[currentVertex] = Vector<3,float>(normal[0],normal[1],normal[2]);
                currentVertex++;

                if (currentVertex > 2) {
                    currentVertex = 0;
                    try {
                        FacePtr face = FacePtr(new Face(vertices[0], vertices[1],vertices[2],
                                                        normals[0], normals[1], normals[2]));
                        faces->Add(face);
                    }
                    catch (ArithmeticException e) {
                        logger.warning << "Face caused an arithmetic exception" << logger.end;
                    } 
                    currentFace++;
                }
            }
            currentP++;
        }
        delete[] offsetMap;
    }
    
    return new GeometryNode(faces);
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
    if (node != NULL) return;
  

    //initialize the collada database
    dae = new DAE();

    int err = dae->load(file.data());
    if (err != DAE_OK) {
        logger.warning << "Error opening Collada file: " << file << logger.end;
        return;
    }
  
    node = new SceneNode();

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
        logger.info << "No visual scene instance defined" << logger.end;
        return;
    }


    // process all <node> elements in the visual scene
    daeElement* elm = scene->getInstance_visual_scene()->getUrl().getElement();
    domNode_Array nodeArr = ((domVisual_scene*)elm)->getNode_array();

    for (unsigned int n = 0; n < nodeArr.getCount(); n++) {
        domNode* dNode = nodeArr[n];

        Quaternion<float> q;
        TransformationNode* tnode = new TransformationNode();
        node->AddNode(tnode);
        
        //process <rotate> tags NOT FINISHED!!!
        domRotate_Array rotateArr = dNode->getRotate_array();
        for (unsigned int r = 0; r < rotateArr.getCount(); r++) {
            domFloat4 rotation = rotateArr[r]->getValue();
            //tnode->Rotate(rot)
        } 
        
        // TODO: process transformation info and create a transformation node

        // process each <instance_geometry> element
        domInstance_geometry_Array geomArr = dNode->getInstance_geometry_array();
        for (unsigned int g = 0; g < geomArr.getCount(); g++) {
            elm = geomArr[g]->getUrl().getElement();
            tnode->AddNode(LoadGeometry((domGeometry*)elm));
        }
    }
}

/**
 * Unload the resource.
 * Resets the face collection. Does not delete the face set.
 */
void ColladaResource::Unload() {
    node = NULL;
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
    return node;
}

} // NS Resources
} // NS OpenEngine
