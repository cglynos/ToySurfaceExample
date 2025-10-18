#pragma once

#include <string>
#include <vector>
#include <set>
#include <numeric>

#include <maya/MArgList.h>
#include <maya/MAttributeSpecArray.h>
#include <maya/MAttributeSpec.h>
#include <maya/MAttributeIndex.h>
#include <maya/MDagModifier.h>
#include <maya/MDagPath.h>
#include <maya/MDrawRegistry.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MPoint.h>
#include <maya/MSelectionMask.h>
#include <maya/MShaderManager.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MTypeId.h>

#include <maya/MHWGeometry.h>
#include <maya/MHWGeometryUtilities.h>

#include <maya/MPxCommand.h>
#include <maya/MPxComponentConverter.h>
#include <maya/MPxGeometryOverride.h>
#include <maya/MPxSurfaceShape.h>

#include <maya/MFnDependencyNode.h>
#include <maya/MFnSingleIndexedComponent.h>

#include <maya/MItDependencyNodes.h>


#define CHECK_STATUS_AND_RETURN_IF_FAIL( status_ , message_ )					\
    if( !status_ )																\
    {																			\
        MString error = status_.errorString() + " -- " + MString( message_ );	\
        MGlobal::displayError( "ERROR >> " + error );							\
        return MStatus::kFailure;												\
    }	


struct ToySurfaceModel
{
	typedef std::vector<MFloatVector> CV_List;
	typedef std::vector<unsigned int> FIDX_List;

    static CV_List	 control_vertices;
    static FIDX_List face_indices;

	template< typename T >
	static void set_control_vertices( std::size_t cvs_size_, const T& control_vertices_ )
	{
		control_vertices.reserve( cvs_size_ );
		for (auto&& cv : control_vertices_)
		{
			control_vertices.emplace_back( static_cast<float>(cv.x),
										   static_cast<float>(cv.y),
										   static_cast<float>(cv.z) );
		}
	}

	template< typename T >
	static void set_face_indices( std::size_t fidx_size_,  const T& face_indices_ )
	{
		face_indices.reserve( fidx_size_ );
		for (auto&& idx : face_indices_)
		{
			face_indices.emplace_back( idx );
		}
	}
};
ToySurfaceModel::CV_List     ToySurfaceModel::control_vertices;
ToySurfaceModel::FIDX_List   ToySurfaceModel::face_indices;

ToySurfaceModel model;

struct ToySurfaceVertexComponentConverter : MHWRender::MPxComponentConverter
{
	const MFn::Type					component_type;
	const MSelectionMask			selection_type;
		  MFnSingleIndexedComponent component_fn;
		  MObject                   component_obj;
		  std::vector<int>          control_vertex_ids;

	ToySurfaceVertexComponentConverter( MFn::Type      componentType, 
										MSelectionMask selectionType )
										: 
										MHWRender::MPxComponentConverter(),
										component_type( componentType ),
										selection_type( selectionType )
	{;}

	void initialize( const MHWRender::MRenderItem& renderItem ) override
	{
		component_obj = component_fn.create( component_type );
		control_vertex_ids.resize( model.control_vertices.size() );
		std::iota( std::begin(control_vertex_ids), std::end(control_vertex_ids), 0 );
	}

	void addIntersection( MHWRender::MIntersection& intersection ) override
	{
		const int idx = intersection.index();
		component_fn.addElement( control_vertex_ids[idx] );
	}

	MObject component() override
	{
		return component_obj;
	}

	MSelectionMask selectionMask() const override
	{
		return selection_type;
	}

	static MPxComponentConverter* creator()
	{
		MSelectionMask mask;
		mask.setMask(MSelectionMask::kSelectMeshVerts);
		mask.addMask(MSelectionMask::kSelectPointsForGravity);
		return new ToySurfaceVertexComponentConverter( MFn::kMeshVertComponent, mask );
	}
};


struct ToySurface : MPxSurfaceShape
{
	static MTypeId id;
    static MString drawDbClassification;
    static MString drawRegistrantId;

	void postConstructor() override
	{
		setRenderable( false );
	}

    MatchResult matchComponent( const MSelectionList& item, const MAttributeSpecArray& spec, MSelectionList& list ) override
	{
		MatchResult result = MPxSurfaceShape::kMatchOk;

		for (unsigned i = 0; i < spec.length(); ++i)
		{
			const MAttributeSpec& attrib = spec[i];

			if (attrib.name() == "vtx")
			{
				MAttributeIndex idx = attrib[0];
				int i = 0;
				idx.getValue( i );

				if ( (i < 0) || (i >= static_cast<int>(model.control_vertices.size())) )
				{
					result = MPxSurfaceShape::kMatchInvalidAttributeRange;
				}
				else
				{
					MDagPath shapePath;
					item.getDagPath(0, shapePath);

					MFnSingleIndexedComponent fnComp;
					MObject comp = fnComp.create( MFn::kMeshVertComponent );
					fnComp.addElement( i );
					list.add(shapePath, comp);
				}
			}
			else
			{
				return MPxSurfaceShape::matchComponent( item, spec, list );
			}
		}

		return result;
	}

    bool isBounded() const override
	{
		return true;
	}

	MBoundingBox boundingBox() const override
	{
		MBoundingBox bbox;
		for (auto&& cv : model.control_vertices)
			bbox.expand( MPoint(cv.x,cv.y,cv.z) );
		return bbox;
	}

	MSelectionMask getShapeSelectionMask() const override
	{
		return MSelectionMask( MSelectionMask::kSelectMeshes );
	}

	MSelectionMask getComponentSelectionMask() const override
	{
		MSelectionMask mask( MSelectionMask::kSelectMeshVerts );
		mask.addMask( MSelectionMask::kSelectPointsForGravity );
		return mask;
	}

    static MStatus initialize()
	{
		return MStatus::kSuccess;
	}

    static void* creator()
    {
        return new ToySurface();
    }
};
MTypeId ToySurface::id( 0x00080029 );
MString ToySurface::drawDbClassification("drawdb/geometry/ToySurfaceGeomOverride");
MString ToySurface::drawRegistrantId("ToySurfacePlugin");


struct ToySurfaceGeomOverride : MHWRender::MPxGeometryOverride
{
	ToySurface*     fMesh;
	std::set<int>   fActiveVerticesSet;

	ToySurfaceGeomOverride( const MObject& obj ) : MPxGeometryOverride(obj),
												   fMesh(NULL)
	{
		MStatus status;
		MFnDependencyNode node(obj, &status);
		if (status)
		{
			fMesh = dynamic_cast<ToySurface*>(node.userNode());
		}
	}

    ~ToySurfaceGeomOverride() override = default;

    MHWRender::DrawAPI supportedDrawAPIs() const override
	{
		return (MHWRender::kOpenGL | MHWRender::kDirectX11 | MHWRender::kOpenGLCoreProfile);
	}

	void updateDG() override
	{
		fActiveVerticesSet.clear();

		if (fMesh && fMesh->hasActiveComponents())
		{
			MObjectArray activeComponents = fMesh->activeComponents();
			if (activeComponents.length() > 0)
			{
				MFnSingleIndexedComponent fnComponent( activeComponents[0] );
				if (fnComponent.elementCount())
				{
					MIntArray activeIds;
					fnComponent.getElements( activeIds );

					if(fnComponent.componentType() == MFn::kMeshVertComponent)
					{
						fActiveVerticesSet.insert( activeIds.begin(), activeIds.end() );
					}
				}
			}
		}
	}

	void updateSurface( const MDagPath& path, MRenderItemList& renderItems, const MHWRender::MShaderManager* shaderMgr )
	{
		const MString shadedSurfaceName = "ToySurface_shaded";
		auto renderItemIndex = renderItems.indexOf( shadedSurfaceName );
		if (renderItemIndex < 0)
		{
			MHWRender::MRenderItem* shadedRenderItem = MHWRender::MRenderItem::Create( shadedSurfaceName, 
																					   MHWRender::MRenderItem::MaterialSceneItem,
																					   MHWRender::MGeometry::kTriangles );
			shadedRenderItem->setDrawMode( MHWRender::MGeometry::kAll );
			shadedRenderItem->depthPriority( MHWRender::MRenderItem::sDormantFilledDepthPriority );
			MShaderInstance* shader = shaderMgr->getStockShader( MShaderManager::k3dSolidShader );
			if (shader)
			{
				const float colour[] = { 1.0f, 0.25f, 0.25f, 1.0f };
				shader->setParameter( "solidColor", colour );
				shadedRenderItem->setShader( shader );
				shaderMgr->releaseShader( shader );
			}
			renderItems.append( shadedRenderItem );
			shadedRenderItem->enable( true );
		}
	}

	void updateDormantVertices( const MDagPath& path, MHWRender::MRenderItemList& list, const MHWRender::MShaderManager* shaderMgr )
	{
		static const MString sVertexItemName = "ToySurfaceDormantVertices";	

		MHWRender::MRenderItem* vertexItem = NULL;
		int index = list.indexOf(sVertexItemName);
		if (index < 0)
		{
			vertexItem = MHWRender::MRenderItem::Create( sVertexItemName, MHWRender::MRenderItem::DecorationItem, MHWRender::MGeometry::kPoints);
			vertexItem->setDrawMode(MHWRender::MGeometry::kAll);

			MSelectionMask vertexAndGravity( MSelectionMask::kSelectMeshVerts );
			vertexAndGravity.addMask( MSelectionMask::kSelectPointsForGravity );
			vertexItem->setSelectionMask( vertexAndGravity );
			vertexItem->depthPriority( MHWRender::MRenderItem::sDormantPointDepthPriority );
			list.append(vertexItem);

			MHWRender::MShaderInstance* shader = shaderMgr->getStockShader( MHWRender::MShaderManager::k3dFatPointShader );
			if (shader)
			{
				const float colour[] = { 0.5f, 0.0f, 0.8f, 1.0f };
				shader->setParameter( "solidColor", colour );
				shader->setParameter( "pointSize", 4.0f );
				vertexItem->setShader( shader );
				shaderMgr->releaseShader( shader );
			}
		}
		else
		{
			vertexItem = list.itemAt(index);
		}

		MHWRender::DisplayStatus displayStatus = MHWRender::MGeometryUtilities::displayStatus(path);
		if (displayStatus == MHWRender::kHilite)
		{
			vertexItem->enable(true);
		}
		else
		{
			vertexItem->enable(false);
		}

	}

	void updateActiveVertices( const MDagPath& path, MHWRender::MRenderItemList& list, const MHWRender::MShaderManager* shaderMgr )
	{
		static const MString sActiveVertexItemName = "ToySurfaceActiveVertices";

		MHWRender::MRenderItem* activeItem = NULL;
		int index = list.indexOf(sActiveVertexItemName);
		if (index < 0)
		{
			activeItem = MHWRender::MRenderItem::Create( sActiveVertexItemName, MHWRender::MRenderItem::DecorationItem, MHWRender::MGeometry::kPoints);
			activeItem->setDrawMode(MHWRender::MGeometry::kAll);
			activeItem->depthPriority( MHWRender::MRenderItem::sActivePointDepthPriority );
			list.append(activeItem);

			MHWRender::MShaderInstance* shader = shaderMgr->getStockShader( MHWRender::MShaderManager::k3dFatPointShader );
			if (shader)
			{
				const float colour[] = { 1.0f, 1.0f, 0.0f, 1.0f };
				shader->setParameter( "solidColor", colour );
				shader->setParameter( "pointSize", 4.0f );
				activeItem->setShader( shader );
				shaderMgr->releaseShader( shader );
			}
		}
		else
		{
			activeItem = list.itemAt(index);
		}

		MHWRender::DisplayStatus displayStatus = MHWRender::MGeometryUtilities::displayStatus(path);
		if ((displayStatus != MHWRender::kHilite) && (displayStatus != MHWRender::kActiveComponent) || (fActiveVerticesSet.empty()))
		{
			activeItem->enable( false );
		}
		else
		{
			activeItem->enable( true );
		}
	}

	void updateRenderItems( const MDagPath& path, MRenderItemList& renderItems ) override
	{
		if (!path.isValid())
			return;

		MRenderer* renderer = MRenderer::theRenderer();
		if (!renderer)
			return;
    
		const MShaderManager* shaderManager = renderer->getShaderManager();
		if (!shaderManager)
			return;

		updateSurface( path, renderItems, shaderManager );
		updateDormantVertices(path, renderItems, shaderManager);
		updateActiveVertices(path, renderItems, shaderManager);
	}

	void populateGeometry( const MGeometryRequirements& requirements, const MRenderItemList& renderItems, MGeometry& data ) override
	{
		if (!fMesh)
			return;
		  
		if (model.control_vertices.empty() || model.face_indices.empty())
			return;

		MVertexBufferDescriptor desc( "", MGeometry::kPosition, MGeometry::kFloat, 3 );
		MVertexBuffer* vb = data.createVertexBuffer( desc );
		if (vb)
		{
			const auto& cvs = model.control_vertices;
		              
			void* buffer = vb->acquire( cvs.size(), true );
			if(buffer)
			{
				const std::size_t bufferSizeInByte = sizeof(ToySurfaceModel::CV_List::value_type) * cvs.size();
				memcpy( buffer, cvs.data(), bufferSizeInByte );
				vb->commit( buffer );
				data.addVertexBuffer(vb);
			}
		}

		const int numItems = renderItems.length();
		for (int i = 0; i < numItems; i++)
		{
			const MHWRender::MRenderItem* item = renderItems.itemAt(i);
			if (!item)
				continue;
		      
			if (item->name() == "ToySurface_shaded")
			{
				MHWRender::MIndexBuffer* indexBuffer = data.createIndexBuffer(MHWRender::MGeometry::kUnsignedInt32);
				if (indexBuffer)
				{
					unsigned int* buffer = static_cast<unsigned int*>( indexBuffer->acquire( model.face_indices.size(), true ) );
					std::copy( std::begin(model.face_indices), std::end(model.face_indices), buffer );
					indexBuffer->commit(buffer);
					item->associateWithIndexBuffer(indexBuffer);
				}
			}

			if (item->name() == "ToySurfaceDormantVertices")
			{
				MHWRender::MIndexBuffer* indexBuffer = data.createIndexBuffer(MHWRender::MGeometry::kUnsignedInt32);
				if (indexBuffer)
				{
					unsigned int* buffer = static_cast<unsigned int*>( indexBuffer->acquire( model.control_vertices.size(), true ) );
					std::iota( buffer, buffer+model.control_vertices.size(), 0 );
					indexBuffer->commit( buffer );
					item->associateWithIndexBuffer( indexBuffer );
				}
			}

			if ((item->name() == "ToySurfaceActiveVertices") && (fActiveVerticesSet.size() > 0))
			{
				MHWRender::MIndexBuffer* indexBuffer = data.createIndexBuffer(MHWRender::MGeometry::kUnsignedInt32);
				if (indexBuffer)
				{
					unsigned int* buffer = static_cast<unsigned int*>( indexBuffer->acquire( fActiveVerticesSet.size(), true ) );
					std::copy( std::cbegin(fActiveVerticesSet), std::cend(fActiveVerticesSet), buffer);
					indexBuffer->commit( buffer );
					item->associateWithIndexBuffer( indexBuffer );
				}
			}
		}
	}

    void cleanUp() override
	{}

	void updateSelectionGranularity( const MDagPath& path, MHWRender::MSelectionContext& selectionContext ) override
	{
		MHWRender::DisplayStatus displayStatus = MHWRender::MGeometryUtilities::displayStatus(path);
		if (displayStatus == MHWRender::kHilite)
		{
			MSelectionMask cmpMask = MGlobal::objectSelectionMask(); 
			if (MGlobal::selectionMode() == MGlobal::kSelectComponentMode)
			{
				cmpMask = MGlobal::componentSelectionMask();

				MSelectionMask supported_cmps( MSelectionMask::kSelectMeshVerts );
				supported_cmps.addMask( MSelectionMask::kSelectPointsForGravity );

				if (cmpMask.intersects(supported_cmps))
				{
					selectionContext.setSelectionLevel( MHWRender::MSelectionContext::kComponent );
				}
			}
		}
	}

	static MStatus registerComponentConverters()
	{
		static const MString sVertexItemName = "ToySurfaceDormantVertices";
		MStatus status = MHWRender::MDrawRegistry::registerComponentConverter(sVertexItemName, ToySurfaceVertexComponentConverter::creator);
		return status;
	}

	static MStatus deregisterComponentConverters()
	{
		static const MString sVertexItemName = "ToySurfaceDormantVertices";
		MStatus status = MHWRender::MDrawRegistry::deregisterComponentConverter( sVertexItemName );
		return status;
	}

	static MPxGeometryOverride* creator( const MObject& obj )
	{
		return new ToySurfaceGeomOverride( obj );
	}
};


bool object_exists( const MTypeId& typeId_, MObject& obj_ )
{
    MItDependencyNodes iter;
    while (!iter.isDone())
	{
        MObject existing_obj = iter.thisNode();
        if (existing_obj.isNull())
			continue;
		else
		{
            MFnDependencyNode fnNode( existing_obj );
			if (fnNode.typeId() == typeId_)
			{
				obj_ = existing_obj;
				return true;
			}
        }
		iter.next();
    }
    return false;
}


struct Create_Toy_Surface : public MPxCommand
{
	MStatus doIt( const MArgList &args_ ) override
	{
		std::vector<MFloatVector> server_control_vertices = { MFloatVector(-1.06757,0.292627,-2.50058),MFloatVector(-0.725968,1.04071,-1.92988),MFloatVector(-0.429184,1.5,-1.43405),MFloatVector(-0.618961,2.5,-1.75111),MFloatVector(-0.637939,3.5,-1.78282),MFloatVector(-0.429184,4.16667,-1.43405),MFloatVector(-0.429184,4.5,-1.43405),MFloatVector(-1.64385,0.292627,-1.91321),MFloatVector(-1.11786,1.04071,-1.53046),MFloatVector(-0.660863,1.5,-1.19792),MFloatVector(-0.953085,2.5,-1.41056),MFloatVector(-0.982307,3.5,-1.43182),MFloatVector(-0.660863,4.16667,-1.19792),MFloatVector(-0.660863,4.5,-1.19792),MFloatVector(-1.86172,0.292627,-0.319601),MFloatVector(-1.26601,1.04071,-0.446769),MFloatVector(-0.74845,1.5,-0.557254),MFloatVector(-1.0794,2.5,-0.486605),MFloatVector(-1.1125,3.5,-0.47954),MFloatVector(-0.74845,4.16667,-0.557254),MFloatVector(-0.74845,4.5,-0.557254),MFloatVector(0,0.292627,1.06653),MFloatVector(0,1.04071,0.49583),MFloatVector(0,1.5,0),MFloatVector(0,2.5,0.317056),MFloatVector(0,3.5,0.348762),MFloatVector(0,4.16667,0),MFloatVector(0,4.5,0),MFloatVector(1.86172,0.292627,-0.319601),MFloatVector(1.26601,1.04071,-0.446769),MFloatVector(0.74845,1.5,-0.557254),MFloatVector(1.0794,2.5,-0.486605),MFloatVector(1.1125,3.5,-0.47954),MFloatVector(0.74845,4.16667,-0.557254),MFloatVector(0.74845,4.5,-0.557254),MFloatVector(1.64385,0.292627,-1.91321),MFloatVector(1.11786,1.04071,-1.53046),MFloatVector(0.660863,1.5,-1.19792),MFloatVector(0.953085,2.5,-1.41056),MFloatVector(0.982307,3.5,-1.43182),MFloatVector(0.660863,4.16667,-1.19792),MFloatVector(0.660863,4.5,-1.19792),MFloatVector(1.06757,0.292627,-2.50058),MFloatVector(0.725968,1.04071,-1.92988),MFloatVector(0.429184,1.5,-1.43405),MFloatVector(0.618961,2.5,-1.75111),MFloatVector(0.637939,3.5,-1.78282),MFloatVector(0.429184,4.16667,-1.43405),MFloatVector(0.429184,4.5,-1.43405) };
		std::vector<unsigned>     server_face_indices     = { 0,1,8,8,7,0,1,2,9,9,8,1,2,3,10,10,9,2,3,4,11,11,10,3,4,5,12,12,11,4,5,6,13,13,12,5,7,8,15,15,14,7,8,9,16,16,15,8,9,10,17,17,16,9,10,11,18,18,17,10,11,12,19,19,18,11,12,13,20,20,19,12,14,15,22,22,21,14,15,16,23,23,22,15,16,17,24,24,23,16,17,18,25,25,24,17,18,19,26,26,25,18,19,20,27,27,26,19,21,22,29,29,28,21,22,23,30,30,29,22,23,24,31,31,30,23,24,25,32,32,31,24,25,26,33,33,32,25,26,27,34,34,33,26,28,29,36,36,35,28,29,30,37,37,36,29,30,31,38,38,37,30,31,32,39,39,38,31,32,33,40,40,39,32,33,34,41,41,40,33,35,36,43,43,42,35,36,37,44,44,43,36,37,38,45,45,44,37,38,39,46,46,45,38,39,40,47,47,46,39,40,41,48,48,47,40 };

		MObject obj = MObject::kNullObj;
		MDagModifier dagModifier;

		if (object_exists( ToySurface::id, obj ))
			dagModifier.deleteNode( obj );

		ToySurfaceModel::set_control_vertices( server_control_vertices.size(), server_control_vertices );
		ToySurfaceModel::set_face_indices( server_face_indices.size(), server_face_indices );

		obj = dagModifier.createNode( ToySurface::id );
		dagModifier.doIt();

		return MStatus::kSuccess;
	}

	static void* creator()
	{
		return new Create_Toy_Surface;
	}
};




