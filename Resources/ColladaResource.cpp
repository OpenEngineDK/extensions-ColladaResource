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



// integration templates
//#include "intGeometry.h"

namespace OpenEngine {
namespace Resources {

using namespace OpenEngine::Logging;
using OpenEngine::Utils::Convert;

// integration template register method

// void intRegisterElements() {
//     intGeometry::registerElement();
// }



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
    ColladaResource::ColladaResource(string file) : file(file), faces(NULL), dae(NULL) {}

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
 * Load a Collada dae scene file.
 *
 * This method parses the file given to the constructor and populates a
 * FaceSet with the data from the file that can be retrieved with
 * GetFaceSet().
 *
 * @see Geometry::FaceSet
 * @see Geometry::Face
 */
void ColladaResource::Load() {

    // check if we have loaded the resource
    if (faces != NULL) return;

    float vertex[3], normal[3];

    // create a new face set
    faces = new FaceSet();

    dae = new DAE();
    //    dae->setIntegrationLibrary(&intRegisterElements);

    int err = dae->load(file.data());
    if (err != DAE_OK) {
        logger.warning << "Error opening Collada file: " << file << logger.end;
        return;
    }
    
    // Find all the geometry dom nodes
    int geomCount = dae->getDatabase()->getElementCount(NULL,COLLADA_ELEMENT_GEOMETRY,NULL);
    logger.info << "Found " << geomCount << " Geometry Nodes." << logger.end;
    
    // Iterate through each domGeometry node (each domGeometry node
    // possibly contain a domMesh child which contains the actual
    // geometric primitives)
    for (int i = 0; i < geomCount; i++) {
        logger.info << "Processing geometry node " << i << "..." << logger.end;
   
        // Retrieve the domMesh node contained in the current domGeometry node
        domGeometry* geom;
        err = dae->getDatabase()->getElement((daeElement**)&geom, i, NULL, COLLADA_ELEMENT_GEOMETRY, NULL);
        if (err != DAE_OK) {
            logger.warning << "Error retrieving geometry element with index: " << i << logger.end;
            continue;
        }
        logger.info << "Name: " << geom->getName() << "." << logger.end;
        domMesh* mesh = geom->getMesh();
           
        // Retrieve an array of domTriangles nodes contained in the mesh 
        domTriangles_Array triangles_arr = mesh->getTriangles_array();
        int triListCount = triangles_arr.getCount(); // number of triangle lists

        logger.info << "Found " << triListCount << " triangle lists." << logger.end;
       
        // for each domTriangles node 
        for (int j = 0; j < triListCount; j++) {

            domTriangles* triangles = triangles_arr[j]; 
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
            for (int k = 0; k < inputCount; k++) {
                InputMap im;
                im.dest = NULL;

                domInputLocalOffset* input = input_arr[k];
                
                
                //const char* semantic = input->getSemantic();
                
                if (strcmp(input->getSemantic(),COMMON_PROFILE_INPUT_VERTEX) == 0) {
                    im.dest = vertex;
                    im.size = 3;
                }
                else if (strcmp(input->getSemantic(),COMMON_PROFILE_INPUT_NORMAL) == 0) {
                    im.dest = normal;
                    im.size = 3;
                }
                
                daeElement* elm = input->getSource().getElement();
                  
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
                
                int offset = input->getOffset();
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
            
            vertex[0] = 0;
            vertex[1] = 0;
            vertex[2] = 0;
            normal[0] = 0;
            normal[1] = 0;
            normal[2] = 0;
            
            while (currentP < pCount) {
                int p = p_arr[currentP];
                for (vector<InputMap>::iterator itr = offsetMap[currentOffset].begin();
                     itr != offsetMap[currentOffset].end();
                     itr++) {
                    InputMap im = *itr;
                    if (im.dest != NULL) {
                        // for each p index we read "size" values beginning from "p*stride"
                        for (int l = 0; l < im.size; l++) {
                            im.dest[l] = im.src[p*im.stride+l];
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
                        catch (Exception e) {
                            logger.warning << "Previous face caused an exception!" << logger.end;
                        } 
                        
                        currentFace++;
                    }

                    vertex[0] = 0;
                    vertex[1] = 0;
                    vertex[2] = 0;
                    normal[0] = 0;
                    normal[1] = 0;
                    normal[2] = 0;
                }

                currentP++;
            }
         
            delete[] offsetMap;
        }
    }
}

/**
 * Unload the resource.
 * Resets the face collection. Does not delete the face set.
 */
void ColladaResource::Unload() {
    faces = NULL;
    if (dae != NULL)
        delete dae;
}

/**
 * Get the face set for the loaded OBJ data.
 *
 * @return Face set
 */
FaceSet* ColladaResource::GetFaceSet() {
    return faces;
}

} // NS Resources
} // NS OpenEngine
