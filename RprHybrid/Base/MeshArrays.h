#pragma once

#include <iostream>
#include <stdint.h>
#include <memory>
#include <vector>
#include <algorithm>

struct ArrayDesc
{
    void* pointer;
    size_t elementSize;
    size_t stride;
    size_t elementCount;
    size_t flags;
};

struct InterleavedArrays
{
    void* baseAddress;
    void* endAddress; // NOT inside the array
    size_t stride;
    std::vector<ArrayDesc> arrays;
};

static std::vector<InterleavedArrays> CombineInterleavedArrays(std::vector<ArrayDesc>&& arrays)
{
    std::vector<InterleavedArrays> result;

    for (ArrayDesc& a : arrays)
    {
        if (a.pointer)
        {
            void* endAddr = static_cast<void*>(static_cast<char*>(a.pointer) + (a.stride * (a.elementCount)));

            bool inserted = false;
            for (InterleavedArrays& ia : result)
            {
                if ((a.pointer >= ia.baseAddress && a.pointer < ia.endAddress) ||
                    (endAddr > ia.baseAddress && endAddr <= ia.endAddress))
                {
                    ia.baseAddress = std::min(ia.baseAddress, a.pointer);
                    ia.endAddress = std::max(ia.endAddress, endAddr);
                    ia.arrays.push_back(a);
                    inserted = true;
                    break;
                }
            }

            if (!inserted)
                result.push_back({ a.pointer, endAddr, a.stride, std::vector<ArrayDesc>({ a }) });
        }
        else
        {
            result.push_back({ nullptr, nullptr, 0, std::vector<ArrayDesc>({ a }) });
        }
    }

    return result;
}

static std::vector<ArrayDesc> CopyMeshArrays(std::vector<ArrayDesc>&& arrays)
{
    // Determine which arrays are interleaved.
    std::vector<InterleavedArrays> interleavedArrays = CombineInterleavedArrays(std::move(arrays));

    // Allocate memory for each interleaved array.
    std::vector<ArrayDesc> result;
    for (auto& ia : interleavedArrays)
    {
        // Allocate new memory block and copy previous memory into it.
        if (ia.baseAddress)
        {
            size_t size = static_cast<char*>(ia.endAddress) - static_cast<char*>(ia.baseAddress);// +ia.stride;
            char* block = new char[size];
            memcpy(block, ia.baseAddress, size);

            // Reconfigure arrays for new memory block.
            for (auto& a : ia.arrays)
            {
                result.push_back({
                    static_cast<void*>(block + (static_cast<char*>(a.pointer) - static_cast<char*>(ia.baseAddress))),
                    a.elementSize,
                    ia.stride,
                    a.elementCount,
                    a.flags
                });
            }
        }
    }

    return result;
}

static void DeleteMeshArrays(FrNode* node)
{
    // Vertex arrays.
    {
		rpr_uint uvdim = node->GetProperty<rpr_uint>(RPR_MESH_UV_DIM);

        // Determine which arrays are interleaved.
        std::vector<InterleavedArrays> interleavedArrays = CombineInterleavedArrays({
            { reinterpret_cast<void*>(node->GetProperty<rpr_float*>(RPR_MESH_VERTEX_ARRAY)), sizeof(rpr_float) * 3, node->GetProperty<size_t>(RPR_MESH_VERTEX_STRIDE), node->GetProperty<size_t>(RPR_MESH_VERTEX_COUNT), 0 },
            { reinterpret_cast<void*>(node->GetProperty<rpr_float*>(RPR_MESH_NORMAL_ARRAY)), sizeof(rpr_float) * 3, node->GetProperty<size_t>(RPR_MESH_NORMAL_STRIDE), node->GetProperty<size_t>(RPR_MESH_NORMAL_COUNT), 1 },
            { reinterpret_cast<void*>(node->GetProperty<rpr_float*>(RPR_MESH_UV_ARRAY)),  sizeof(rpr_float) * uvdim, node->GetProperty<size_t>(RPR_MESH_UV_STRIDE),  node->GetProperty<size_t>(RPR_MESH_UV_COUNT),  2 },
			{ reinterpret_cast<void*>(node->GetProperty<rpr_float*>(RPR_MESH_UV2_ARRAY)), sizeof(rpr_float) * uvdim, node->GetProperty<size_t>(RPR_MESH_UV2_STRIDE), node->GetProperty<size_t>(RPR_MESH_UV2_COUNT), 2 },
        });

        // Each InterleavedArrays instance has a pointer that must be deallocated.
        for (InterleavedArrays& ia : interleavedArrays)
        {
            if (ia.baseAddress)
                delete[] reinterpret_cast<char*>(ia.baseAddress);
        }
    }
    
    auto numFaceVerts = node->GetProperty<rpr_int*>(RPR_MESH_NUM_FACE_VERTICES_ARRAY);
    auto numFaces = node->GetProperty<size_t>(RPR_MESH_POLYGON_COUNT);
    
    size_t indicesCount = 0U;
	for (size_t i = 0; i < numFaces; ++i)
	{
	    indicesCount += numFaceVerts[i];
	}
    
    // Index arrays.
    {
        // Determine which arrays are interleaved.
        std::vector<InterleavedArrays> interleavedArrays = CombineInterleavedArrays({
            { reinterpret_cast<void*>(node->GetProperty<rpr_int*>(RPR_MESH_VERTEX_INDEX_ARRAY)), sizeof(int), node->GetProperty<size_t>(RPR_MESH_VERTEX_INDEX_STRIDE), indicesCount, 0 },
            { reinterpret_cast<void*>(node->GetProperty<rpr_int*>(RPR_MESH_NORMAL_INDEX_ARRAY)), sizeof(int), node->GetProperty<size_t>(RPR_MESH_NORMAL_INDEX_STRIDE), indicesCount, 1 },
            { reinterpret_cast<void*>(node->GetProperty<rpr_int*>(RPR_MESH_UV_INDEX_ARRAY)), sizeof(int), node->GetProperty<size_t>(RPR_MESH_UV_INDEX_STRIDE), indicesCount, 2 },
            { reinterpret_cast<void*>(node->GetProperty<rpr_int*>(RPR_MESH_UV2_INDEX_ARRAY)), sizeof(int), node->GetProperty<size_t>(RPR_MESH_UV2_INDEX_STRIDE), indicesCount, 3 }, // needed to make UnitTest_FrMeshTest.test_feature_multiUV pass -- I believe there is a typo above with RPR_MESH_UV2_COUNT -- flags should be 3 not 2 -- AND I believe this will fail/throw-exception if used on a mesh created with rprContextCreateMesh vs rprContextCreateMeshEx -- also the above GetProperty(RPR_MESH_UV2_ARRAY) will fail if rprContextCreateMesh is used... -- actually I think it will not throw because of FrPropertyFactory::InitializeNodeProperties
        });

        // Each InterleavedArrays instance has a pointer that must be deallocated.
        for (InterleavedArrays& ia : interleavedArrays)
        {
            if (ia.baseAddress)
                delete [] reinterpret_cast<char*>(ia.baseAddress);
        }
    }

    // Num faces.
    delete[] numFaceVerts;
}