/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2014 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/
#ifndef _OgreHlmsPbs_H_
#define _OgreHlmsPbs_H_

#include "OgreHlmsPbsPrerequisites.h"
#include "OgreHlms.h"
#include "OgreConstBufferPool.h"
#include "OgreHeaderPrefix.h"

namespace Ogre
{
    class CompositorShadowNode;
    struct QueuedRenderable;

    /** \addtogroup Component
    *  @{
    */
    /** \addtogroup Material
    *  @{
    */

    class HlmsPbsDatablock;

    /** Physically based shading implementation specfically designed for OpenGL ES 2.0 and other
        RenderSystems which do not support uniform buffers.
    */
    class _OgreHlmsPbsExport HlmsPbs : public Hlms, public ConstBufferPool
    {
        typedef vector<ConstBufferPacked*>::type ConstBufferPackedVec;
        typedef vector<TexBufferPacked*>::type TexBufferPackedVec;
        typedef vector<HlmsDatablock*>::type HlmsDatablockVec;

        struct PassData
        {
            FastArray<TexturePtr>   shadowMaps;
            FastArray<float>    vertexShaderSharedBuffer;
            FastArray<float>    pixelShaderSharedBuffer;

            Matrix4 viewProjMatrix;
            Matrix4 viewMatrix;
        };

        PassData                mPreparedPass;
        ConstBufferPackedVec    mPassBuffers;

        uint32                  mCurrentPassBuffer;     /// Resets every to zero every new frame.
        uint32                  mCurrentConstBuffer;    /// Resets every to zero every new frame.
        uint32                  mCurrentTexBuffer;      /// Resets every to zero every new frame.
        ConstBufferPackedVec    mConstBuffers;
        TexBufferPackedVec      mTexBuffers;

        ConstBufferPool::BufferPool const *mLastBoundPool;

        uint32  *mStartMappedConstBuffer;
        uint32  *mCurrentMappedConstBuffer;
        size_t  mCurrentConstBufferSize;

        float   *mRealStartMappedTexBuffer;
        float   *mStartMappedTexBuffer;
        float   *mCurrentMappedTexBuffer;
        size_t  mCurrentTexBufferSize;
        size_t  mTexBufferAlignment; //Not in bytes, already divided by 4.

        /// Resets every to zero every new buffer (@see unmapTexBuffer and @see mapNextTexBuffer).
        size_t  mTexLastOffset;
        size_t  mLastTexBufferCmdOffset;

        uint32 mLastTextureHash;

        size_t mTextureBufferDefaultSize;

        virtual const HlmsCache* createShaderCacheEntry( uint32 renderableHash,
                                                         const HlmsCache &passCache,
                                                         uint32 finalHash,
                                                         const QueuedRenderable &queuedRenderable );

        virtual HlmsDatablock* createDatablockImpl( IdString datablockName,
                                                    const HlmsMacroblock *macroblock,
                                                    const HlmsBlendblock *blendblock,
                                                    const HlmsParamVec &paramVec );

        void setDetailMapProperties( HlmsPbsDatablock *datablock, PiecesMap *inOutPieces );
        void setTextureProperty( IdString propertyName, HlmsPbsDatablock *datablock,
                                 PbsTextureTypes texType );

        virtual void calculateHashForPreCreate( Renderable *renderable, PiecesMap *inOutPieces );
        virtual void calculateHashForPreCaster( Renderable *renderable, PiecesMap *inOutPieces );

        /// For compatibility reasons with D3D11 and GLES3, Const buffers are mapped.
        /// Once we're done with it (even if we didn't fully use it) we discard it
        /// and get a new one. We will at least have to get a new one on every pass.
        /// This is affordable since common Const buffer limits are of 64kb.
        /// At the next frame we restart mCurrentConstBuffer to 0.
        void unmapConstBuffer(void);

        /// Warning: Calling this function affects BOTH mCurrentConstBuffer and mCurrentTexBuffer
        DECL_MALLOC uint32* mapNextConstBuffer( CommandBuffer *commandBuffer );

        /// Texture buffers are treated differently than Const buffers. We first map it.
        /// Once we're done with it, we save our progress (in mTexLastOffset) and in the
        /// next pass start where we left off (i.e. if we wrote to the first 2MB chunk,
        /// start mapping from 2MB onwards). Only when the buffer is full, we get a new
        /// Tex Buffer.
        /// At the next frame we restart mCurrentTexBuffer to 0.
        ///
        /// Tex Buffers can be as big as 128MB, thus "restarting" with another 128MB
        /// buffer on every pass is too expensive. This strategy benefits low level RS
        /// like GL3+ and D3D11.1* (Windows 8) and D3D12; whereas on D3D11 and GLES3
        /// drivers dynamic mapping may discover we're writing to a region not in use
        /// or may internally use a new buffer (wasting memory space).
        ///
        /// (*) D3D11.1 allows using MAP_NO_OVERWRITE for texture buffers.
        void unmapTexBuffer( CommandBuffer *commandBuffer );
        DECL_MALLOC float* mapNextTexBuffer( CommandBuffer *commandBuffer, size_t minimumSizeBytes );

        /** Rebinds the texture buffer. Finishes the last bind command to the tbuffer.
        @param resetOffset
            When true, the tbuffer will be offsetted so that the shader samples
            from 0 at the current offset in mCurrentMappedTexBuffer
            WARNING: mCurrentMappedTexBuffer may be modified due to alignment.
            mStartMappedTexBuffer & mCurrentTexBufferSize will always be modified
        @param minimumTexBufferSize
            If resetOffset is true and the remaining space in the currently mapped
            tbuffer is less than minimumSizeBytes, we will call mapNextTexBuffer
        */
        void rebindTexBuffer( CommandBuffer *commandBuffer, bool resetOffset = false,
                              size_t minimumSizeBytes = 1 );

        void destroyAllBuffers(void);

    public:
        HlmsPbs( Archive *dataFolder );
        ~HlmsPbs();

        virtual void _changeRenderSystem( RenderSystem *newRs );

        virtual HlmsCache preparePassHash( const Ogre::CompositorShadowNode *shadowNode,
                                           bool casterPass, bool dualParaboloid,
                                           SceneManager *sceneManager );

        virtual uint32 fillBuffersFor( const HlmsCache *cache, const QueuedRenderable &queuedRenderable,
                                       bool casterPass, uint32 lastCacheHash,
                                       uint32 lastTextureHash );

        virtual uint32 fillBuffersFor( const HlmsCache *cache, const QueuedRenderable &queuedRenderable,
                                       bool casterPass, uint32 lastCacheHash,
                                       CommandBuffer *commandBuffer );

        virtual void preCommandBufferExecution( CommandBuffer *commandBuffer );
        virtual void postCommandBufferExecution( CommandBuffer *commandBuffer );

        virtual void frameEnded(void);

        /// Changes the default suggested size for the texture buffer.
        /// Actual size may be lower if the GPU can't honour the request.
        void setTextureBufferDefaultSize( size_t defaultSize );
    };

    struct _OgreHlmsPbsExport PbsProperty
    {
        static const IdString HwGammaRead;
        static const IdString HwGammaWrite;
        static const IdString SignedIntTex;
        static const IdString MaterialsPerBuffer;

        static const IdString NumTextures;
        static const IdString DiffuseMap;
        static const IdString NormalMapTex;
        static const IdString SpecularMap;
        static const IdString RoughnessMap;
        static const IdString EnvProbeMap;
        static const IdString DetailWeightMap;
        static const IdString DetailMap0;
        static const IdString DetailMap1;
        static const IdString DetailMap2;
        static const IdString DetailMap3;
        static const IdString DetailMapNm0;
        static const IdString DetailMapNm1;
        static const IdString DetailMapNm2;
        static const IdString DetailMapNm3;

        static const IdString NormalMap;

        static const IdString FresnelScalar;

        static const IdString NormalWeight;
        static const IdString NormalWeightTex;
        static const IdString NormalWeightDetail0;
        static const IdString NormalWeightDetail1;
        static const IdString NormalWeightDetail2;
        static const IdString NormalWeightDetail3;

        static const IdString DetailWeights;
        static const IdString DetailOffsetsD0;
        static const IdString DetailOffsetsD1;
        static const IdString DetailOffsetsD2;
        static const IdString DetailOffsetsD3;
        static const IdString DetailOffsetsN0;
        static const IdString DetailOffsetsN1;
        static const IdString DetailOffsetsN2;
        static const IdString DetailOffsetsN3;

        static const IdString UvDiffuse;
        static const IdString UvNormal;
        static const IdString UvSpecular;
        static const IdString UvRoughness;
        static const IdString UvDetailWeight;

        static const IdString UvDetail0;
        static const IdString UvDetail1;
        static const IdString UvDetail2;
        static const IdString UvDetail3;

        static const IdString UvDetailNm0;
        static const IdString UvDetailNm1;
        static const IdString UvDetailNm2;
        static const IdString UvDetailNm3;

        static const IdString DetailMapsDiffuse;
        static const IdString DetailMapsNormal;
        static const IdString FirstValidDetailMapNm;

        static const IdString BlendModeIndex0;
        static const IdString BlendModeIndex1;
        static const IdString BlendModeIndex2;
        static const IdString BlendModeIndex3;

        static const IdString *UvSourcePtrs[NUM_PBSM_SOURCES];
        static const IdString *BlendModes[4];
        static const IdString *DetailNormalWeights[4];
        static const IdString *DetailOffsetsDPtrs[4];
        static const IdString *DetailOffsetsNPtrs[4];
        static const IdString *DetailMaps[4];
        static const IdString *DetailMapsNm[4];
    };

    /** @} */
    /** @} */

}

#include "OgreHeaderSuffix.h"

#endif