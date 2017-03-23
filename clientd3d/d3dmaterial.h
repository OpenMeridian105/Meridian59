// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
#ifndef __D3DMATERIAL_H__
#define __D3DMATERIAL_H__

// Material functions.

// blank
Bool D3DMaterialNone(d3d_render_chunk_new *pPool);

// world
Bool D3DMaterialWorldPool(d3d_render_pool_new *pPool);
Bool D3DMaterialWorldPacket(d3d_render_packet_new *pPacket, d3d_render_cache_system *pCacheSystem);
Bool D3DMaterialSkyboxChunk(d3d_render_chunk_new *pChunk);
Bool D3DMaterialFloorCeilDynamicChunk(d3d_render_chunk_new *pChunk);
Bool D3DMaterialFloorCeilStaticChunk(d3d_render_chunk_new *pChunk);
Bool D3DMaterialWallDynamicChunk(d3d_render_chunk_new *pChunk);
Bool D3DMaterialWallStaticChunk(d3d_render_chunk_new *pChunk);
Bool D3DMaterialWallMaskPool(d3d_render_pool_new *pPool);
Bool D3DMaterialMaskChunk(d3d_render_chunk_new *pChunk);

// lmaps
Bool D3DMaterialLMapDynamicPool(d3d_render_pool_new *pPool);
Bool D3DMaterialLMapDynamicPacket(d3d_render_packet_new *pPacket, d3d_render_cache_system *pCacheSystem);
Bool D3DMaterialLMapFloorCeilDynamicChunk(d3d_render_chunk_new *pChunk);
Bool D3DMaterialLMapWallDynamicChunk(d3d_render_chunk_new *pChunk);
Bool D3DMaterialLMapFloorCeilStaticChunk(d3d_render_chunk_new *pChunk);
Bool D3DMaterialLMapWallStaticChunk(d3d_render_chunk_new *pChunk);

// objects
Bool D3DMaterialObjectPool(d3d_render_pool_new *pPool);
Bool D3DMaterialObjectPacket(d3d_render_packet_new *pPacket, d3d_render_cache_system *pCacheSystem);
Bool D3DMaterialObjectChunk(d3d_render_chunk_new *pChunk);
Bool D3DMaterialTargetedObjectChunk(d3d_render_chunk_new *pChunk);

// invisible objects
Bool D3DMaterialObjectInvisiblePool(d3d_render_pool_new *pPool);
Bool D3DMaterialObjectInvisiblePacket(d3d_render_packet_new *pPacket, d3d_render_cache_system *pCacheSystem);
Bool D3DMaterialObjectInvisibleChunk(d3d_render_chunk_new *pChunk);

// effects
Bool D3DMaterialEffectPool(d3d_render_pool_new *pPool);
Bool D3DMaterialEffectPacket(d3d_render_packet_new *pPacket, d3d_render_cache_system *pCacheSystem);
Bool D3DMaterialEffectChunk(d3d_render_chunk_new *pChunk);

// blur
Bool D3DMaterialBlurPool(d3d_render_pool_new *pPool);
Bool D3DMaterialBlurPacket(d3d_render_packet_new *pPacket, d3d_render_cache_system *pCacheSystem);
Bool D3DMaterialBlurChunk(d3d_render_chunk_new *pChunk);

// particles
Bool D3DMaterialParticlePool(d3d_render_pool_new *pPool);
Bool D3DMaterialParticlePacket(d3d_render_packet_new *pPacket, d3d_render_cache_system *pCacheSystem);
Bool D3DMaterialParticleChunk(d3d_render_chunk_new *pChunk);

#endif
