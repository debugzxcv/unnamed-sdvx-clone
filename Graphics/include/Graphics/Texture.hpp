#pragma once
#include <Graphics/ResourceTypes.hpp>
#include <memory>
#include <array>
#include <string>

namespace Graphics
{
	enum class TextureWrap
	{
		Repeat,
		Mirror,
		Clamp,
	};

	enum class TextureFormat
	{
		RGBA8,
		D32,
		Invalid
	};

	template <class T, uint8_t N>
	struct ImageData {
		ImageData(uint32_t width, uint32_t height)
			: width_{width},
			height_{height},
			component_count_{width * height * N},
			data_{std::make_unique<T[]>(component_count_)} {}
		ImageData(uint32_t width, uint32_t height, T* data)
			: width_{width},
			height_{height},
			component_count_{width * height * N},
			from_raw_pointer_{true},
			data_(data) {}
		~ImageData() {
			if (from_raw_pointer_) {
				data_.release();
			}
		}
		ImageData(ImageData const&) = default;
		ImageData& operator=(ImageData const&) = default;
		ImageData(ImageData&&) = default;
		ImageData& operator=(ImageData&&) = default;
		uint32_t constexpr width() const noexcept { return width_; }
		uint32_t constexpr height() const noexcept { return height_; }
		uint64_t constexpr size() const noexcept { return component_count_; }
		// uint64_t constexpr pixels() const noexcept { return width_ * height_; }
		T* data() noexcept { return data_.get(); }
		T const* data() const noexcept { return data_.get(); }
		// T *release() noexcept { return data_.release(); }
		// T const *release() const noexcept { return data_.release(); }
		std::array<T, N> ReadPixel(uint32_t x, uint32_t y) const {
			// assert(data_ != nullptr);

			std::array<T, N> dst;
			std::memcpy(dst.data(), data_.get() + (y * width_ + x) * N, N);
			return dst;
			// auto dst = std::make_unique<T[]>(N);
			// std::memcpy(dst.get(), data_.get() + (y * width_ + x) * N, N);
			// return dst.release();

			// auto dst = new T[N];
			// std::memcpy(dst, data_.get() + (y * width_ + x) * N, N);
			// return dst;
		}

	private:
		uint32_t width_, height_;
		uint64_t component_count_;
		bool from_raw_pointer_;
		std::unique_ptr<T[]> data_;
	};

	class ImageRes;
	/*
		OpenGL texture wrapper, can be created from an Image object or raw data
	*/
	class TextureRes
	{
	public:
		virtual ~TextureRes() = default;
		static Ref<TextureRes> Create(class OpenGL* gl);
		static Ref<TextureRes> Create(class OpenGL* gl, Ref<class ImageRes> image);
		static Ref<TextureRes> CreateFromFrameBuffer(class OpenGL* gl, const Vector2i& resolution);
	public:
		virtual void Init(Vector2i size, TextureFormat format = TextureFormat::RGBA8) = 0;
		virtual void SetData(Vector2i size, void* pData) = 0;
		virtual void SetFromFrameBuffer(Vector2i pos = { 0, 0 }) = 0;
		virtual void SetMipmaps(bool enabled) = 0;
		virtual void SetMipmaps(int maxlevel = 0, double gamma = 2.2) = 0;
		virtual void SetFilter(bool enabled, bool mipFiltering = true, float anisotropic = 1.0f) = 0;
		virtual const Vector2i& GetSize() const = 0;

		// Gives the aspect ratio correct height for a given width
		float CalculateHeight(float width);
		// Gives the aspect ratio correct width for a given height
		float CalculateWidth(float height);

		// Binds the texture to a given texture unit (default = 0)
		virtual void Bind(uint32 index = 0) = 0;
		virtual uint32 Handle() = 0;
		virtual void SetWrap(TextureWrap u, TextureWrap v) = 0;
		virtual TextureFormat GetFormat() const = 0;
	};

	typedef Ref<TextureRes> Texture;

	DEFINE_RESOURCE_TYPE(Texture, TextureRes);
}