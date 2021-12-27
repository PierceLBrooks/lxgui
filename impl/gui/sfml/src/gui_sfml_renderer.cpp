#include "lxgui/impl/gui_sfml_renderer.hpp"
#include "lxgui/impl/gui_sfml_atlas.hpp"
#include "lxgui/impl/gui_sfml_material.hpp"
#include "lxgui/impl/gui_sfml_rendertarget.hpp"
#include "lxgui/impl/gui_sfml_font.hpp"
#include "lxgui/impl/gui_sfml_vertexcache.hpp"
#include <lxgui/gui_quad.hpp>
#include <lxgui/gui_matrix4.hpp>
#include <lxgui/gui_out.hpp>
#include <lxgui/gui_exception.hpp>
#include <lxgui/utils_string.hpp>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/VertexBuffer.hpp>

namespace lxgui {
namespace gui {
namespace sfml
{
renderer::renderer(sf::RenderWindow& mWindow) : mWindow_(mWindow),
    mWindowDimensions_(mWindow.getSize().x, mWindow.getSize().y)
{
}

std::string renderer::get_name() const
{
    return "SFML";
}

void renderer::begin_(std::shared_ptr<gui::render_target> pTarget) const
{
    if (pCurrentTarget_ || pCurrentSFMLTarget_)
        throw gui::exception("gui::sfml::renderer", "Missing call to end()");

    if (pTarget)
    {
        pCurrentTarget_ = std::static_pointer_cast<sfml::render_target>(pTarget);
        pCurrentTarget_->begin();
        pCurrentSFMLTarget_ = pCurrentTarget_->get_render_texture();
    }
    else
    {
        sf::FloatRect mVisibleArea(0, 0, mWindowDimensions_.x, mWindowDimensions_.y);
        mWindow_.setView(sf::View(mVisibleArea));
        pCurrentSFMLTarget_ = &mWindow_;
    }
}

void renderer::end_() const
{
    if (pCurrentTarget_)
        pCurrentTarget_->end();

    pCurrentTarget_ = nullptr;
    pCurrentSFMLTarget_ = nullptr;
}

void renderer::set_view_(const matrix4f& mViewMatrix) const
{
    static const float RAD_TO_DEG = 180.0f/std::acos(-1.0f);

    float fScaleX = std::sqrt(mViewMatrix(0,0)*mViewMatrix(0,0) + mViewMatrix(1,0)*mViewMatrix(1,0));
    float fScaleY = std::sqrt(mViewMatrix(0,1)*mViewMatrix(0,1) + mViewMatrix(1,1)*mViewMatrix(1,1));
    float fAngle = std::atan2(mViewMatrix(0,1)/fScaleY, mViewMatrix(0,0)/fScaleX)*RAD_TO_DEG;

    sf::View mView;
    mView.setCenter(sf::Vector2f(-mViewMatrix(3,0)/fScaleX, -mViewMatrix(3,1)/fScaleY));
    mView.rotate(fAngle);
    mView.setSize(sf::Vector2f(2.0f/fScaleX, 2.0/fScaleY));

    pCurrentSFMLTarget_->setView(mView);
}

matrix4f renderer::get_view() const
{
    matrix4f mCurrentViewMatrix = matrix4f(pCurrentSFMLTarget_->getView().getTransform().getMatrix());
    if (!pCurrentTarget_)
    {
        // Rendering to main screen, flip Y
        for (std::size_t i = 0; i < 4; ++i)
            mCurrentViewMatrix(i,1) *= -1.0f;
    }

    return mCurrentViewMatrix;
}

void renderer::render_quads_(const gui::material* pMaterial, const std::vector<std::array<vertex,4>>& lQuadList) const
{
    static const std::array<std::size_t, 6> lIDs = {{0, 1, 2, 2, 3, 0}};
    static const std::size_t uiNumVertices = lIDs.size();

    const sfml::material* pMat = static_cast<const sfml::material*>(pMaterial);

    vector2f mTexDims(1.0f, 1.0f);
    if (pMat)
        mTexDims = vector2f(pMat->get_canvas_dimensions());

    sf::VertexArray mArray(sf::PrimitiveType::Triangles, lIDs.size() * lQuadList.size());
    for (std::size_t k = 0; k < lQuadList.size(); ++k)
    {
        const std::array<vertex,4>& mVertices = lQuadList[k];
        for (std::size_t i = 0; i < uiNumVertices; ++i)
        {
            const std::size_t j = lIDs[i];
            sf::Vertex& mSFVertex = mArray[k*uiNumVertices + i];
            const vertex& mVertex = mVertices[j];
            const float a = mVertex.col.a;

            mSFVertex.position.x  = mVertex.pos.x;
            mSFVertex.position.y  = mVertex.pos.y;
            mSFVertex.texCoords.x = mVertex.uvs.x*mTexDims.x;
            mSFVertex.texCoords.y = mVertex.uvs.y*mTexDims.y;
            mSFVertex.color.r     = mVertex.col.r*a*255; // Premultipled alpha
            mSFVertex.color.g     = mVertex.col.g*a*255; // Premultipled alpha
            mSFVertex.color.b     = mVertex.col.b*a*255; // Premultipled alpha
            mSFVertex.color.a     = a*255;
        }
    }

    sf::RenderStates mState;
    mState.blendMode = sf::BlendMode(sf::BlendMode::One, sf::BlendMode::OneMinusSrcAlpha); // Premultiplied alpha
    if (pMat)
        mState.texture = pMat->get_texture();
    pCurrentSFMLTarget_->draw(mArray, mState);
}

sf::Transform to_sfml(const matrix4f& mMatrix)
{
    return sf::Transform(
        mMatrix(0,0), mMatrix(1,0), mMatrix(3,0),
        mMatrix(0,1), mMatrix(1,1), mMatrix(3,1),
        0.0, 0.0, 1.0
    );
}

void renderer::render_cache_(const gui::material* pMaterial, const gui::vertex_cache& mCache,
    const matrix4f& mModelTransform) const
{
    throw gui::exception("gui::sfml::renderer", "SFML does not support vertex caches.");

#if 0
    const sfml::material* pMat = static_cast<const sfml::material*>(pMaterial);
    const sfml::vertex_cache& mSFCache = static_cast<const sfml::vertex_cache&>(mCache);

    // Note: the following will not work correctly, as vertex_cache has texture coordinates
    // normalised, but sf::RenderTarget::draw assumes coordinates in pixels.
    // https://github.com/SFML/SFML/issues/1773
    sf::RenderStates mState;
    mState.blendMode = sf::BlendMode(sf::BlendMode::One, sf::BlendMode::OneMinusSrcAlpha); // Premultiplied alpha
    if (pMat)
        mState.texture = pMat->get_texture();
    mState.transform = to_sfml(mModelTransform);
    pCurrentSFMLTarget_->draw(mSFCache.get_impl(), 0, mSFCache.get_num_vertex(), mState);
#endif
}

std::shared_ptr<gui::material> renderer::create_material_(const std::string& sFileName, material::filter mFilter) const
{
    return std::make_shared<sfml::material>(sFileName, material::wrap::REPEAT, mFilter);
}

std::shared_ptr<gui::atlas> renderer::create_atlas_(material::filter mFilter) const
{
    return std::make_shared<sfml::atlas>(*this, mFilter);
}

std::size_t renderer::get_texture_max_size() const
{
    return sf::Texture::getMaximumSize();
}

bool renderer::is_texture_atlas_supported() const
{
    return true;
}

bool renderer::is_texture_vertex_color_supported() const
{
    return true;
}

std::shared_ptr<gui::material> renderer::create_material(const vector2ui& mDimensions,
    const ub32color* pPixelData, material::filter mFilter) const
{
    std::shared_ptr<sfml::material> pTex = std::make_shared<sfml::material>(
        mDimensions, false, material::wrap::REPEAT, mFilter);

    pTex->update_texture(pPixelData);

    return std::move(pTex);
}

std::shared_ptr<gui::material> renderer::create_material(
    std::shared_ptr<gui::render_target> pRenderTarget, const bounds2f& mLocation) const
{
    auto pTex = std::static_pointer_cast<sfml::render_target>(pRenderTarget)->get_material().lock();
    if (mLocation == pRenderTarget->get_rect())
    {
        return std::move(pTex);
    }
    else
    {
        return std::make_shared<sfml::material>(
            pTex->get_render_texture()->getTexture(), mLocation, pTex->get_filter());
    }
}

std::shared_ptr<gui::render_target> renderer::create_render_target(
    const vector2ui& mDimensions, material::filter mFilter) const
{
    return std::make_shared<sfml::render_target>(mDimensions, mFilter);
}

std::shared_ptr<gui::font> renderer::create_font_(const std::string& sFontFile, std::size_t uiSize,
    std::size_t uiOutline, const std::vector<code_point_range>& lCodePoints,
    char32_t uiDefaultCodePoint) const
{
    return std::make_shared<sfml::font>(sFontFile, uiSize, uiOutline, lCodePoints, uiDefaultCodePoint);
}

bool renderer::is_vertex_cache_supported() const
{
#if defined(SFML_HAS_NORMALISED_COORDINATES_VBO)
    // Requires https://github.com/SFML/SFML/pull/1807
    return sf::VertexBuffer::isAvailable();
#else
    return false;
#endif
}

std::shared_ptr<gui::vertex_cache> renderer::create_vertex_cache(gui::vertex_cache::type mType) const
{
#if defined(SFML_HAS_NORMALISED_COORDINATES_VBO)
    // Requires https://github.com/SFML/SFML/pull/1807
    return std::make_shared<sfml::vertex_cache>(mType);
#else
    throw gui::exception("gui::sfml::renderer", "SFML does not support vertex caches.");
#endif
}

void renderer::notify_window_resized(const vector2ui& mNewDimensions)
{
    mWindowDimensions_ =  mNewDimensions;
}

}
}
}