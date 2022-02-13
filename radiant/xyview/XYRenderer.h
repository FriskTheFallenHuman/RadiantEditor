#pragma once

#include "irender.h"
#include "render/RenderableCollectorBase.h"
#include "imap.h"

/// RenderableCollector implementation for the ortho view
class XYRenderer : 
    public render::RenderableCollectorBase
{
public:
    struct HighlightShaders
    {
        ShaderPtr selectedShader;
        ShaderPtr selectedShaderGroup;
        ShaderPtr mergeActionShaderAdd;
        ShaderPtr mergeActionShaderChange;
        ShaderPtr mergeActionShaderRemove;
        ShaderPtr mergeActionShaderConflict;
    };

private:
    RenderStateFlags _globalstate;

    IMap::EditMode _editMode;

    const HighlightShaders& _shaders;

public:
    XYRenderer(RenderStateFlags globalstate, const HighlightShaders& shaders) :
        _globalstate(globalstate),
        _editMode(GlobalMapModule().getEditMode()),
        _shaders(shaders)
    {}

    bool supportsFullMaterials() const override
    {
        return false;
    }

    void addRenderable(Shader& shader,
                       const OpenGLRenderable& renderable,
                       const Matrix4& localToWorld,
                       const LitObject* /* litObject */,
                       const IRenderEntity* entity = nullptr) override
    {
        addHighlightRenderable(renderable, localToWorld);

        shader.addRenderable(renderable, localToWorld, entity);
    }

    void addHighlightRenderable(const OpenGLRenderable& renderable, const Matrix4& localToWorld) override
    {
        if (_editMode == IMap::EditMode::Merge)
        {
            if (_flags & Highlight::Flags::MergeAction)
            {
                // This is a merge-relevant node that should be rendered in a special colour
                const auto& mergeShader = (_flags & Highlight::Flags::MergeActionAdd) != 0 ? _shaders.mergeActionShaderAdd :
                    (_flags & Highlight::Flags::MergeActionRemove) != 0 ? _shaders.mergeActionShaderRemove :
                    (_flags & Highlight::Flags::MergeActionConflict) != 0 ? _shaders.mergeActionShaderConflict : _shaders.mergeActionShaderChange;

                if (mergeShader)
                {
                    mergeShader->addRenderable(renderable, localToWorld, nullptr);
                }
            }

            // Elements can still be selected in merge mode
            if ((_flags & Highlight::Flags::Primitives) != 0)
            {
                _shaders.selectedShader->addRenderable(renderable, localToWorld, nullptr);
            }

            return;
        }

        // Regular editing mode, add all highlighted nodes to the corresponding shader
        if ((_flags & Highlight::Flags::Primitives) != 0)
        {
            if ((_flags & Highlight::Flags::GroupMember) != 0)
            {
                _shaders.selectedShaderGroup->addRenderable(renderable, localToWorld, nullptr);
            }
            else
            {
                _shaders.selectedShader->addRenderable(renderable, localToWorld, nullptr);
            }
        }
    }

    void render(const Matrix4& modelview, const Matrix4& projection, const VolumeTest& view)
    {
        GlobalRenderSystem().render(RenderViewType::OrthoView, _globalstate, modelview, projection, Vector3(0,0,0), view);
    }
}; // class XYRenderer
