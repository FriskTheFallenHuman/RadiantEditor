#include "Face.h"

#include "ivolumetest.h"
#include "ifilter.h"
#include "itextstream.h"
#include "irenderable.h"

#include "math/Matrix3.h"
#include "shaderlib.h"
#include "texturelib.h"
#include "Winding.h"
#include "selection/algorithm/Texturing.h"

#include "Brush.h"
#include "BrushNode.h"
#include "BrushModule.h"

// The structure that is saved in the undostack
class Face::SavedState :
    public IUndoMemento
{
public:
    FacePlane::SavedState _planeState;
    TextureProjection _texdefState;
    std::string _materialName;

    SavedState(const Face& face) :
        _planeState(face.getPlane()),
        _texdefState(face.getProjection()),
        _materialName(face.getShader())
    {}

    virtual ~SavedState() {}

    void exportState(Face& face) const
    {
        _planeState.exportState(face.getPlane());
        face.setShader(_materialName);
        face.getProjection().assign(_texdefState);
    }
};

Face::Face(Brush& owner) :
    _owner(owner),
    _shader(texdef_name_default(), _owner.getBrushNode().getRenderSystem()),
    _undoStateSaver(nullptr),
    _faceIsVisible(true)
{
    setupSurfaceShader();

    m_plane.initialiseFromPoints(
        Vector3(0, 0, 0), Vector3(64, 0, 0), Vector3(0, 64, 0)
    );
    planeChanged();
    shaderChanged();
}

Face::Face(
    Brush& owner,
    const Vector3& p0,
    const Vector3& p1,
    const Vector3& p2,
    const std::string& shader,
    const TextureProjection& projection
) :
    _owner(owner),
    _shader(shader, _owner.getBrushNode().getRenderSystem()),
    _texdef(projection),
    _undoStateSaver(nullptr),
    _faceIsVisible(true)
{
    setupSurfaceShader();
    m_plane.initialiseFromPoints(p0, p1, p2);
    planeChanged();
    shaderChanged();
}

Face::Face(Brush& owner, const Plane3& plane) :
    _owner(owner),
    _shader("", _owner.getBrushNode().getRenderSystem()),
    _undoStateSaver(nullptr),
    _faceIsVisible(true)
{
    setupSurfaceShader();
    m_plane.setPlane(plane);
    planeChanged();
    shaderChanged();
}

Face::Face(Brush& owner, const Plane3& plane, const Matrix4& texdef,
           const std::string& shader) :
    _owner(owner),
    _shader(shader, _owner.getBrushNode().getRenderSystem()),
    _undoStateSaver(nullptr),
    _faceIsVisible(true)
{
    setupSurfaceShader();
    m_plane.setPlane(plane);

    _texdef.matrix = TextureMatrix(texdef);

    planeChanged();
    shaderChanged();
}

Face::Face(Brush& owner, const Face& other) :
    IFace(other),
    IUndoable(other),
    _owner(owner),
    m_plane(other.m_plane),
    _shader(other._shader.getMaterialName(), _owner.getBrushNode().getRenderSystem()),
    _texdef(other.getProjection()),
    _undoStateSaver(nullptr),
    _faceIsVisible(other._faceIsVisible)
{
    setupSurfaceShader();
    planepts_assign(m_move_planepts, other.m_move_planepts);
    planeChanged();
}

Face::~Face()
{
    _surfaceShaderRealised.disconnect();
}

void Face::setupSurfaceShader()
{
    _surfaceShaderRealised = _shader.signal_Realised().connect(
        sigc::mem_fun(*this, &Face::realiseShader));

    // If we're already in realised state, call realiseShader right away
    if (_shader.isRealised())
    {
        realiseShader();
    }
}

IBrush& Face::getBrush()
{
    return _owner;
}

Brush& Face::getBrushInternal()
{
    return _owner;
}

void Face::planeChanged()
{
    revertTransform();
    _owner.onFacePlaneChanged();
}

void Face::realiseShader()
{
    _owner.onFaceShaderChanged();
}

void Face::connectUndoSystem(IMapFileChangeTracker& changeTracker)
{
    assert(!_undoStateSaver);

    _shader.setInUse(true);

    _undoStateSaver = GlobalUndoSystem().getStateSaver(*this, changeTracker);
}

void Face::disconnectUndoSystem(IMapFileChangeTracker& changeTracker)
{
    assert(_undoStateSaver);
    _undoStateSaver = nullptr;
    GlobalUndoSystem().releaseStateSaver(*this);

    _shader.setInUse(false);
}

void Face::undoSave()
{
    if (_undoStateSaver)
    {
        _undoStateSaver->save(*this);
    }
}

// undoable
IUndoMementoPtr Face::exportState() const
{
    return IUndoMementoPtr(new SavedState(*this));
}

void Face::importState(const IUndoMementoPtr& data)
{
    undoSave();

    std::static_pointer_cast<SavedState>(data)->exportState(*this);

    planeChanged();
    _owner.onFaceConnectivityChanged();
    texdefChanged();
    _owner.onFaceShaderChanged();
}

void Face::flipWinding() {
    m_plane.reverse();
    planeChanged();
}

bool Face::intersectVolume(const VolumeTest& volume) const
{
    if (!m_winding.empty())
    {
        const Plane3& plane = m_planeTransformed.getPlane();
        return volume.TestPlane(Plane3(plane.normal(), -plane.dist()));
    }
    else
    {
        // Empty winding, return false
        return false;
    }
}

bool Face::intersectVolume(const VolumeTest& volume, const Matrix4& localToWorld) const
{
    if (m_winding.size() > 0)
    {
        return volume.TestPlane(Plane3(plane3().normal(), -plane3().dist()), localToWorld);
    }
    else
    {
        // Empty winding, return false
        return false;
    }
}

void Face::renderWireframe(RenderableCollector& collector, const Matrix4& localToWorld,
    const IRenderEntity& entity) const
{
    collector.addRenderable(*entity.getWireShader(), m_winding, localToWorld,
                            nullptr, &entity);
}

void Face::setRenderSystem(const RenderSystemPtr& renderSystem)
{
    _shader.setRenderSystem(renderSystem);

    // Update the visibility flag, we might have switched shaders
    const ShaderPtr& shader = _shader.getGLShader();

    if (shader)
    {
        _faceIsVisible = shader->getMaterial()->isVisible();
    }
    else
    {
        _faceIsVisible = false; // no shader => not visible
    }
}

void Face::translate(const Vector3& translation)
{
    if (GlobalBrush().textureLockEnabled())
    {
        m_texdefTransformed.transformLocked(_shader.getWidth(), _shader.getHeight(),
            m_plane.getPlane(), Matrix4::getTranslation(translation));
    }

    m_planeTransformed.translate(translation);
    _owner.onFacePlaneChanged();
    updateWinding();
}

void Face::transform(const Matrix4& matrix)
{
    if (GlobalBrush().textureLockEnabled())
    {
        m_texdefTransformed.transformLocked(_shader.getWidth(), _shader.getHeight(), m_plane.getPlane(), matrix);
    }

    // Transform the FacePlane using the given matrix
    m_planeTransformed.transform(matrix);
    _owner.onFacePlaneChanged();
    updateWinding();
}

void Face::assign_planepts(const PlanePoints planepts)
{
    m_planeTransformed.initialiseFromPoints(
        planepts[0], planepts[1], planepts[2]
    );
    _owner.onFacePlaneChanged();
    updateWinding();
}

/// \brief Reverts the transformable state of the brush to identity.
void Face::revertTransform()
{
    m_planeTransformed = m_plane;
    planepts_assign(m_move_planeptsTransformed, m_move_planepts);
    m_texdefTransformed = _texdef;
    updateWinding();
}

void Face::freezeTransform() {
    undoSave();
    m_plane = m_planeTransformed;
    planepts_assign(m_move_planepts, m_move_planeptsTransformed);
    _texdef = m_texdefTransformed;
    updateWinding();
}

void Face::updateWinding() {
    m_winding.updateNormals(m_plane.getPlane().normal());
}

void Face::update_move_planepts_vertex(std::size_t index, PlanePoints planePoints) {
    std::size_t numpoints = getWinding().size();
    ASSERT_MESSAGE(index < numpoints, "update_move_planepts_vertex: invalid index");

    std::size_t opposite = getWinding().opposite(index);
    std::size_t adjacent = getWinding().wrap(opposite + numpoints - 1);
    planePoints[0] = getWinding()[opposite].vertex;
    planePoints[1] = getWinding()[index].vertex;
    planePoints[2] = getWinding()[adjacent].vertex;
    // winding points are very inaccurate, so they must be quantised before using them to generate the face-plane
    planepts_quantise(planePoints, GRID_MIN);
}

void Face::snapto(float snap) {
    if (contributes()) {
        PlanePoints planePoints;
        update_move_planepts_vertex(0, planePoints);
        planePoints[0].snap(snap);
        planePoints[1].snap(snap);
        planePoints[2].snap(snap);
        assign_planepts(planePoints);
        freezeTransform();
        SceneChangeNotify();
        if (!m_plane.getPlane().isValid()) {
            rError() << "WARNING: invalid plane after snap to grid\n";
        }
    }
}

void Face::testSelect(SelectionTest& test, SelectionIntersection& best) {
    m_winding.testSelect(test, best);
}

void Face::testSelect_centroid(SelectionTest& test, SelectionIntersection& best) {
    test.TestPoint(m_centroid, best);
}

void Face::shaderChanged()
{
    EmitTextureCoordinates();
    _owner.onFaceShaderChanged();

    // Update the visibility flag, but leave out the contributes() check
    const ShaderPtr& shader = getFaceShader().getGLShader();

    if (shader)
    {
        _faceIsVisible = shader->getMaterial()->isVisible();
    }
    else
    {
        _faceIsVisible = false; // no shader => not visible
    }

    planeChanged();
    SceneChangeNotify();
}

const std::string& Face::getShader() const
{
    return _shader.getMaterialName();
}

void Face::setShader(const std::string& name)
{
    undoSave();
    _shader.setMaterialName(name);
    shaderChanged();
}

void Face::revertTexdef()
{
    m_texdefTransformed = _texdef;
}

void Face::texdefChanged()
{
    revertTexdef();
    EmitTextureCoordinates();

    // Fire the signal to update the Texture Tools
    signal_texdefChanged().emit();
}

const TextureProjection& Face::getProjection() const
{
    return _texdef;
}

TextureProjection& Face::getProjection()
{
    return _texdef;
}

Matrix4 Face::getProjectionMatrix()
{
    return getProjection().getTransform();
}

void Face::setProjectionMatrix(const Matrix4& projection)
{
    getProjection().setTransform(projection);
    texdefChanged();
}

void Face::GetTexdef(TextureProjection& projection) const
{
    projection = _texdef;
}

void Face::SetTexdef(const TextureProjection& projection)
{
    undoSave();
    _texdef.assign(projection);
    texdefChanged();
}

void Face::setTexdef(const TexDef& texDef)
{
    TextureProjection projection;

    // Construct the BPTexDef out of the TexDef by using the according constructor
    projection.matrix = TextureMatrix(texDef);

    // The bprimitive texdef needs to be scaled using our current texture dims
    auto width = static_cast<double>(_shader.getWidth());
    auto height = static_cast<double>(_shader.getHeight());

    projection.matrix.coords[0][0] /= width;
    projection.matrix.coords[0][1] /= width;
    projection.matrix.coords[0][2] /= width;
    projection.matrix.coords[1][0] /= height;
    projection.matrix.coords[1][1] /= height;
    projection.matrix.coords[1][2] /= height;

    SetTexdef(projection);
}

ShiftScaleRotation Face::getShiftScaleRotation() const
{
    auto texdef = _texdef.matrix.getFakeTexCoords();
    auto ssr = texdef.toShiftScaleRotation();

    // These values are going to show up in the Surface Inspector, so
    // we need to make some adjustments:
   
    // We want the shift values appear in pixels of the editor image,
    // so scale up the UV values by the editor image dimensions
    ssr.shift[0] *= _shader.getWidth();
    ssr.shift[1] *= _shader.getHeight();

    // We only need to display shift values in the range of the texture dimensions
    ssr.shift[0] = float_mod(ssr.shift[0], _shader.getWidth());
    ssr.shift[1] = float_mod(ssr.shift[1], _shader.getHeight());

    // Surface Inspector wants to display values such that scale == 1.0 means:
    // a 512-unit wide face can display the full 512px of the editor image.
    // The corresponding texture matrix transform features a scale value like 1/512
    // to scale the 512 XYZ coord down to 1.0 in UV space.
    // Now, getFakeTexCoords() yields the reciprocal 1/scale (=> 512), to have larger scale
    // values correspond to a higher "texture zoom" factor (is more intuitive that way):
    // => 1024 in getFakeTexcoords() means a 512 editor image appears twice as large visually, 
    // even though the UV coords shrunk only span half the range.
    // We divide by the image dims to receive the 1.0-like values we want to see in the entry box.
    ssr.scale[0] /= _shader.getWidth();
    ssr.scale[1] /= _shader.getHeight();

    return ssr;
#if 0
    TextureProjection curProjection = _texdef;

    // Multiply the texture dimensions to the projection matrix such that
    // the shift/scale/rotation represent pixel values within the image.
    Vector2 shaderDims(_shader.getWidth(), _shader.getHeight());

    TextureMatrix bpTexDef = curProjection.matrix;
    bpTexDef.applyShaderDimensions(static_cast<std::size_t>(shaderDims[0]), static_cast<std::size_t>(shaderDims[1]));

    // Calculate the "fake" texture properties (shift/scale/rotation)
    TexDef texdef = bpTexDef.getFakeTexCoords();

    if (shaderDims != Vector2(0, 0))
    {
        // normalize again to hide the ridiculously high scale values that get created when using texlock
        texdef.normalise(shaderDims[0], shaderDims[1]);
    }

    return texdef.getShiftScaleRotation();
#endif
}

void Face::setShiftScaleRotation(const ShiftScaleRotation& ssr)
{
    // We need to do the opposite adjustments as in Face::getShiftScaleRotation()
    // The incoming values are scaled up and down, respectively.
    auto texdef = TexDef::CreateFromShiftScaleRotation(ssr);

    // Scale the pixel value in SSR to relative UV coords
    texdef.setShift({ texdef.getShift().x() / _shader.getWidth(),
                      texdef.getShift().y() / _shader.getHeight() });

    // Add the texture dimensions to the scale.
    texdef.setScale({ texdef.getScale().x() * _shader.getWidth(),
                      texdef.getScale().y() * _shader.getHeight() });
   
    // Construct the BPTexDef out of this TexDef
    TextureProjection projection;
    projection.matrix = TextureMatrix(texdef);

    SetTexdef(projection);
}

void Face::applyShaderFromFace(const Face& other)
{
    // Retrieve the textureprojection from the source face
    TextureProjection projection;
    other.GetTexdef(projection);

    setShader(other.getShader());
    SetTexdef(projection);

    // The list of shared vertices
    std::vector<Winding::const_iterator> thisVerts, otherVerts;

    // Let's see whether this face is sharing any 3D coordinates with the other one
    for (Winding::const_iterator i = other.m_winding.begin(); i != other.m_winding.end(); ++i) {
        for (Winding::const_iterator j = m_winding.begin(); j != m_winding.end(); ++j) {
            // Check if the vertices are matching
            if (math::isNear(j->vertex, i->vertex, 0.001))
            {
                // Match found, add to list
                thisVerts.push_back(j);
                otherVerts.push_back(i);
            }
        }
    }

    if (thisVerts.empty() || thisVerts.size() != otherVerts.size()) {
        return; // nothing to do
    }

    // Calculate the distance in texture space of the first shared vertices
    Vector2 dist = thisVerts[0]->texcoord - otherVerts[0]->texcoord;

    // Shift the texture to match
    shiftTexdef(static_cast<float>(dist.x()), static_cast<float>(dist.y()));
}

void Face::setTexDefFromPoints(const Vector3 points[3], const Vector2 uvs[3])
{
    // Calculate the texture projection for these desired set of UVs and XYZ

    // The texture projection matrix is applied to the vertices after they have been
    // transformed by the axis base transform (which depends on this face's normal):
    // T * AB * vertex = UV
    // 
    // Applying AB to the vertices will yield: T * P = texcoord
    // with P containing the axis-based transformed vertices.
    // 
    // If the above should be solved for T, expanding the above multiplication 
    // sets up six equations to calculate the 6 unknown components of T.
    // 
    // We can arrange the 6 equations in matrix form: T * A = B
    // T is the 3x3 texture matrix.
    // A contains the XY coords in its columns (Z is ignored since we 
    // applied the axis base), B contains the UV coords in its columns.
    // The third component of all columns in both matrices is 1.
    // 
    // We can solve the above by inverting A: T = B * inv(A)

    // Get the axis base for this face, we need the XYZ points in that state
    // to reverse-calculate the desired texture transform
    auto axisBase = getBasisTransformForNormal(getPlane3().normal());

    // Rotate the three incoming world vertices into the local face plane
    Vector3 localPoints[] =
    {
        axisBase * points[0],
        axisBase * points[1],
        axisBase * points[2],
    };

    // Arrange the XYZ coords into the columns of matrix A
    auto xyz = Matrix3::byColumns(localPoints[0].x(), localPoints[0].y(), 1,
        localPoints[1].x(), localPoints[1].y(), 1,
        localPoints[2].x(), localPoints[2].y(), 1);

    auto uv = Matrix3::byColumns(uvs[0].x(), uvs[0].y(), 1,
        uvs[1].x(), uvs[1].y(), 1,
        uvs[2].x(), uvs[2].y(), 1);

    auto textureMatrix = uv * xyz.getFullInverse();

    m_texdefTransformed.setTransform(textureMatrix);

    EmitTextureCoordinates();

    // Fire the signal to update the Texture Tools
    signal_texdefChanged().emit();
}

void Face::shiftTexdef(float s, float t)
{
    undoSave();
    _texdef.shift(s, t);
    texdefChanged();
}

void Face::shiftTexdefByPixels(float sPixels, float tPixels)
{
    // Scale down the s,t translation using the active texture dimensions
    shiftTexdef(sPixels / _shader.getWidth(), tPixels / _shader.getHeight());
}

void Face::scaleTexdef(float sFactor, float tFactor)
{
    selection::algorithm::TextureScaler::ScaleFace(*this, { sFactor, tFactor });
}

void Face::rotateTexdef(float angle)
{
    selection::algorithm::TextureRotator::RotateFace(*this, degrees_to_radians(angle));
}

void Face::fitTexture(float s_repeat, float t_repeat) {
    undoSave();
    _texdef.fitTexture(_shader.getWidth(), _shader.getHeight(), m_plane.getPlane().normal(), m_winding, s_repeat, t_repeat);
    texdefChanged();
}

void Face::flipTexture(unsigned int flipAxis)
{
    selection::algorithm::TextureFlipper::FlipFace(*this, flipAxis);
}

void Face::alignTexture(AlignEdge align)
{
    undoSave();
    _texdef.alignTexture(align, m_winding);
    texdefChanged();
}

void Face::EmitTextureCoordinates() {
    m_texdefTransformed.emitTextureCoordinates(m_winding, plane3().normal(), Matrix4::getIdentity());
}

void Face::applyDefaultTextureScale()
{
    _texdef.matrix.addScale(_shader.getWidth(), _shader.getHeight());
    texdefChanged();
}

const Vector3& Face::centroid() const {
    return m_centroid;
}

void Face::construct_centroid() {
    // Take the plane and let the winding calculate the centroid
    m_centroid = m_winding.centroid(plane3());
}

const Winding& Face::getWinding() const {
    return m_winding;
}
Winding& Face::getWinding() {
    return m_winding;
}

const Plane3& Face::plane3() const
{
    _owner.onFaceEvaluateTransform();
    return m_planeTransformed.getPlane();
}

const Plane3& Face::getPlane3() const
{
    return m_plane.getPlane();
}

FacePlane& Face::getPlane() {
    return m_plane;
}
const FacePlane& Face::getPlane() const {
    return m_plane;
}

Matrix4 Face::getTexDefMatrix() const
{
    return _texdef.matrix.getTransform();
}

SurfaceShader& Face::getFaceShader() {
    return _shader;
}
const SurfaceShader& Face::getFaceShader() const {
    return _shader;
}

bool Face::contributes() const {
    return m_winding.size() > 2;
}

bool Face::is_bounded() const {
    for (Winding::const_iterator i = m_winding.begin(); i != m_winding.end(); ++i) {
        if (i->adjacent == brush::c_brush_maxFaces) {
            return false;
        }
    }
    return true;
}

void Face::normaliseTexture() {
    undoSave();

    Winding::const_iterator nearest = m_winding.begin();

    // Find the vertex with the minimal distance to the origin
    for (Winding::const_iterator i = m_winding.begin(); i != m_winding.end(); ++i) {
        if (nearest->texcoord.getLength() > i->texcoord.getLength()) {
            nearest = i;
        }
    }

    Vector2 texcoord = nearest->texcoord;

    // The floored values
    Vector2 floored(floor(fabs(texcoord[0])), floor(fabs(texcoord[1])));

    // The signs of the original texcoords (needed to know which direction it should be shifted)
    Vector2 sign(texcoord[0]/fabs(texcoord[0]), texcoord[1]/fabs(texcoord[1]));

    Vector2 shift;
    shift[0] = (fabs(texcoord[0]) > 1.0E-4) ? -floored[0] * sign[0] * _shader.getWidth() : 0.0f;
    shift[0] = (fabs(texcoord[1]) > 1.0E-4) ? -floored[1] * sign[1] * _shader.getHeight() : 0.0f;

    // Shift the texture (note the minus sign, the FaceTexDef negates it yet again).
    _texdef.shift(static_cast<float>(-shift[0]), static_cast<float>(shift[1]));

    texdefChanged();
}

bool Face::isVisible() const
{
    return _faceIsVisible;
}

void Face::updateFaceVisibility()
{
    _faceIsVisible = contributes() && getFaceShader().getGLShader()->getMaterial()->isVisible();
}

sigc::signal<void>& Face::signal_texdefChanged()
{
    static sigc::signal<void> _sigTexdefChanged;
    return _sigTexdefChanged;
}
