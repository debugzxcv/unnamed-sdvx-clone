#include "stdafx.h"
#include "OpenGL.hpp"
#include "Texture.hpp"
#include "Image.hpp"
#include <Graphics/ResourceManagers.hpp>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

namespace Graphics
{

	class Texture_Impl : public TextureRes
	{
		uint32 m_texture = 0;
		TextureWrap m_wmode[2] = { TextureWrap::Repeat };
		TextureFormat m_format = TextureFormat::Invalid;
		Vector2i m_size;
		bool m_filter = true;
		bool m_mipFilter = true;
		bool m_mipmaps = false;
		float m_anisotropic = 1.0f;
		void* m_data = nullptr;
		OpenGL* m_gl = nullptr;
		std::vector<ImageData<uint8_t, 4>> m_mipmapChain {};

	public:
		Texture_Impl(OpenGL* gl)
		{
			m_gl = gl;
		}
		~Texture_Impl()
		{
			if(!m_mipmapChain.empty())
				m_mipmapChain.clear();
			if(m_texture)
				glDeleteTextures(1, &m_texture);
		}
		bool Init()
		{
			glGenTextures(1, &m_texture);
			if(m_texture == 0)
				return false;
			return true;
		}
		virtual void Init(Vector2i size, TextureFormat format)
		{
			m_format = format;
			m_size = size;

			uint32 ifmt = -1;
			uint32 fmt = -1;
			uint32 type = -1;
			if(format == TextureFormat::D32)
			{
				ifmt = GL_DEPTH_COMPONENT32;
				fmt = GL_DEPTH_COMPONENT;
				type = GL_FLOAT;
			}
			else if(format == TextureFormat::RGBA8)
			{
				ifmt = GL_RGBA8;
				fmt = GL_RGBA;
				type = GL_UNSIGNED_BYTE;
			}
			else
			{
				assert(false);
			}

			glBindTexture(GL_TEXTURE_2D, m_texture);
			glTexImage2D(GL_TEXTURE_2D, 0, ifmt, size.x, size.y, 0, fmt, type, nullptr);
			glBindTexture(GL_TEXTURE_2D, 0);

			UpdateFilterState();
			UpdateWrap();
		}
		virtual void SetFromFrameBuffer(Vector2i pos = { 0, 0 })
		{
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			glReadBuffer(GL_BACK);
			glBindTexture(GL_TEXTURE_2D, m_texture);
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pos.x, pos.y, m_size.x, m_size.y);
			GLenum err;
			bool errored = false;
			while ((err = glGetError()) != GL_NO_ERROR)
			{			
				Logf("OpenGL Error: 0x%p", Logger::Severity::Error, err);
				errored = true;
			}
			//assert(!errored);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		virtual void SetData(Vector2i size, void* pData)
		{
			m_format = TextureFormat::RGBA8;
			m_size = size;
			m_data = pData;
			glBindTexture(GL_TEXTURE_2D, m_texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_data);
			glBindTexture(GL_TEXTURE_2D, 0);

			UpdateFilterState();
			UpdateWrap();
		}
		void UpdateFilterState()
		{
			glBindTexture(GL_TEXTURE_2D, m_texture);
			if(!m_mipmaps)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_filter ? GL_LINEAR : GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_filter ? GL_LINEAR : GL_NEAREST);
			}
			else
			{
				if(m_mipFilter)
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_filter ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR);
				else
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_filter ? GL_LINEAR : GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_filter ? GL_LINEAR : GL_NEAREST);
			}
			if(GL_TEXTURE_MAX_ANISOTROPY_EXT)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, m_anisotropic);
			}
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		virtual void SetFilter(bool enabled, bool mipFiltering, float anisotropic)
		{
			m_mipFilter = mipFiltering;
			m_filter = enabled;
			m_anisotropic = anisotropic;
			assert(m_anisotropic >= 1.0f && m_anisotropic <= 16.0f);
			UpdateFilterState();
		}
		virtual void SetMipmaps(bool enabled)
		{
			if(enabled)
			{
				glBindTexture(GL_TEXTURE_2D, m_texture);
				glGenerateMipmap(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, 0);
			}
			m_mipmaps = enabled;
			UpdateFilterState();
		}
		virtual void SetMipmaps(int maxlevel/* = 0 */, double gamma/* = 2.2 */)
		{
			glBindTexture(GL_TEXTURE_2D, m_texture);
			m_mipmapChain = GenerateMipmap(ImageData<uint8_t, 4>(m_size.x, m_size.y, static_cast<uint8_t*>(m_data)), maxlevel, gamma);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, m_mipmapChain.size());
			for(size_t i = 0; i < m_mipmapChain.size(); ++i){
				auto const& imageData = m_mipmapChain.at(i);
				glTexImage2D(GL_TEXTURE_2D, i + 1, GL_RGBA8, imageData.width(), imageData.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, static_cast<void const*>(imageData.data()));
			}
			glBindTexture(GL_TEXTURE_2D, 0);
			m_mipmaps = true;
			UpdateFilterState();
		}
		virtual const Vector2i& GetSize() const
		{
			return m_size;
		}
		virtual void Bind(uint32 index)
		{
			glActiveTexture(GL_TEXTURE0 + index);
			glBindTexture(GL_TEXTURE_2D, m_texture);

		}
		virtual uint32 Handle()
		{
			return m_texture;
		}

		virtual void SetWrap(TextureWrap u, TextureWrap v) override
		{
			m_wmode[0] = u;
			m_wmode[1] = v;
			UpdateWrap();
		}

		void UpdateWrap()
		{
			uint32 wmode[] = {
				GL_REPEAT,
				GL_MIRRORED_REPEAT,
				GL_CLAMP_TO_EDGE,
			};

			glBindTexture(GL_TEXTURE_2D, m_texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wmode[(size_t)m_wmode[0]]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wmode[(size_t)m_wmode[1]]);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		TextureFormat GetFormat() const
		{
			return m_format;
		}

		static std::vector<ImageData<uint8_t, 4>> GenerateMipmap(ImageData<uint8_t, 4> const& imageData,
														int maxlevel,
														float gamma) {
			std::vector<ImageData<uint8_t, 4>> mipmapChain{};
			uint32_t const w{imageData.width()};
			uint32_t const h{imageData.height()};
			if (maxlevel > 0) {
				maxlevel = std::min(maxlevel, static_cast<int>(std::floor(std::log2(std::max(w, h)))));
			} else if (maxlevel < 0) {
				maxlevel = std::floor(std::log2(std::max(w, h))) + maxlevel;
			} else {
				maxlevel = std::floor(std::log2(std::max(w, h)));
			}

			for (auto l{1}; l <= maxlevel; ++l) {
				uint32_t x, y, x0, y0, x1, y1;
				auto const prevlevel{l - 1};
				auto const pw{std::max(1u, w >> prevlevel)};
				auto const ph{std::max(1u, h >> prevlevel)};
				auto const cw{std::max(1u, w >> l)};
				auto const ch{std::max(1u, h >> l)};
				ImageData<uint8_t, 4> mipmap{cw, ch};
				auto curIndex{0};
				auto const& sampleData{prevlevel == 0 ? imageData : mipmapChain.at(prevlevel - 1)};
				for (y = 0; y < ch; ++y) {
					y0 = y << 1;
					y1 = std::min(y0 + 1, ph - 1);
					for (x = 0; x < cw; ++x) {
						x0 = x << 1;
						x1 = std::min(x0 + 1, pw - 1);

						// sampling
						std::array<std::array<uint32_t const, 2> const, 4> const samplePoints{{
							{{x0, y0}}, {{x1, y0}},
							{{x0, y1}}, {{x1, y1}}
						}};
						std::array<std::array<uint8_t, 4>, 4> pixels_;
						std::transform(
							samplePoints.begin(), samplePoints.end(), pixels_.begin(),
							[&sampleData](auto const& p) { return sampleData.ReadPixel(p[0], p[1]); });

						// to linear
						std::array<std::array<double, 4>, 4> pixels_d;
						std::transform(
							pixels_.begin(), pixels_.end(), pixels_d.begin(), [gamma](auto const& pixel_in) {
								std::array<double, 4> pixel_out;
								std::transform(
									pixel_in.begin(), pixel_in.end(), pixel_out.begin(),
									[gamma](auto const component) {
										return std::pow(component, gamma);
									});
								return pixel_out;
							});

						// average 4 pixels
						std::array<double, 4> const result_d{std::reduce(
							pixels_d.begin(), pixels_d.end(), std::array<double, 4>{},
							[](auto const& a, auto const& b) {
								std::array<double, 4> ret;
								std::transform(
									a.begin(), a.end(), b.begin(), ret.begin(),
									[](auto const lhs, auto const rhs) { return (lhs + rhs); });
								return ret;
							})};

						// to gamma
						std::transform(
							result_d.begin(), result_d.end(), mipmap.data() + curIndex,
							[gamma](auto const component) {
								return std::pow(component * .25, 1 / gamma);
							});

						curIndex += 4;
					}
				}
				mipmapChain.emplace_back(std::move(mipmap));
			}
			return mipmapChain;
		}

	};

	Texture TextureRes::Create(OpenGL* gl)
	{
		Texture_Impl* pImpl = new Texture_Impl(gl);
		if(pImpl->Init())
		{
			return GetResourceManager<ResourceType::Texture>().Register(pImpl);
		}
		else
		{
			delete pImpl; pImpl = nullptr;
		}
		return Texture();
	}
	Texture TextureRes::Create(OpenGL* gl, Image image)
	{
		if(!image)
			return Texture();
		Texture_Impl* pImpl = new Texture_Impl(gl);
		if(pImpl->Init())
		{
			pImpl->SetData(image->GetSize(), image->GetBits());
			return GetResourceManager<ResourceType::Texture>().Register(pImpl);
		}
		else
		{
			delete pImpl;
			return Texture();
		}
	}

	Texture TextureRes::CreateFromFrameBuffer(class OpenGL* gl, const Vector2i& resolution)
	{
		Texture_Impl* pImpl = new Texture_Impl(gl);
		if (pImpl->Init())
		{
			pImpl->Init(resolution, TextureFormat::RGBA8);
			pImpl->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);
			pImpl->SetFromFrameBuffer();
			return GetResourceManager<ResourceType::Texture>().Register(pImpl);
		}
		else
		{
			delete pImpl; pImpl = nullptr;
		}
		return Texture();
	}

	float TextureRes::CalculateHeight(float width)
	{
		Vector2 size = GetSize();
		float aspect = size.y / size.x;
		return aspect * width;
	}

	float TextureRes::CalculateWidth(float height)
	{
		Vector2 size = GetSize();
		float aspect = size.x / size.y;
		return aspect * height;
	}



}
