#pragma once

#include "itraceable.h"
#include "modelskin.h"

#include "model/ModelNodeBase.h"
#include "MD5Model.h"
#include "registry/CachedKey.h"
#include "RenderableMD5Skeleton.h"

#include <sigc++/connection.h>

namespace md5
{

constexpr const char* const RKEY_RENDER_SKELETON = "user/ui/md5/renderSkeleton";

class MD5ModelNode :
	public model::ModelNodeBase,
	public model::ModelNode,
	public SelectionTestable,
	public SkinnedModel,
	public ITraceable
{
	MD5ModelPtr _model;

	// The name of this model's skin
	std::string _skin;

    // The default skin in case _skin is set to an empty string
	std::string _defaultSkin;

    sigc::connection _animationUpdateConnection;
    sigc::connection _modelShadersChangedConnection;

    registry::CachedKey<bool> _showSkeleton;

    RenderableMD5Skeleton _renderableSkeleton;

public:
	MD5ModelNode(const MD5ModelPtr& model);
    ~MD5ModelNode() override;

	// ModelNode implementation
	const model::IModel& getIModel() const override;
	model::IModel& getIModel() override;
	bool hasModifiedScale() override;
	Vector3 getModelScale() override;

	// returns the contained model
	void setModel(const MD5ModelPtr& model);
	const MD5ModelPtr& getModel() const;

	// Bounded implementation
	const AABB& localAABB() const override;

	std::string name() const override;

	// SelectionTestable implementation
	void testSelect(Selector& selector, SelectionTest& test) override;

	// Traceable implementation
	bool getIntersection(const Ray& ray, Vector3& intersection) override;

	// Renderable implementation
    void onPreRender(const VolumeTest& volume) override;

	// Returns the name of the currently active skin
	std::string getSkin() const override;
	void skinChanged(const std::string& newSkinName) override;
    void setDefaultSkin(const std::string& defaultSkin) override;

protected:
    void createRenderableSurfaces() override;

private:
    void onModelAnimationUpdated();
    void onModelShadersChanged();
};

} // namespace
