#pragma once
#include "core/common.h"

#include <dxgiformat.h>

namespace dds
{

    enum DDS_PIXELFORMAT_FLAGS
    {
        DDPF_ALPHAPIXELS = 0x1,		// Texture contains alpha data; dwRGBAlphaBitMask contains valid data.
        DDPF_ALPHA = 0x2,			// Used in some older DDS files for alpha channel only uncompressed data (dwRGBBitCount contains the alpha channel bitcount; dwABitMask contains valid data)
        DDPF_FOURCC = 0x4,			// Texture contains compressed RGB data; dwFourCC contains valid data.
        DDPF_RGB = 0x40,			// Texture contains uncompressed RGB data; dwRGBBitCount and the RGB masks (dwRBitMask, dwGBitMask, dwBBitMask) contain valid data.
        DDPF_YUV = 0x200,			// Used in some older DDS files for YUV uncompressed data (dwRGBBitCount contains the YUV bit count; dwRBitMask contains the Y mask, dwGBitMask contains the U mask, dwBBitMask contains the V mask)
        DDPF_LUMINANCE = 0x20000	// Used in some older DDS files for single channel color uncompressed data (dwRGBBitCount contains the luminance channel bit count; dwRBitMask contains the channel mask). Can be combined with DDPF_ALPHAPIXELS for a two channel DDS file.
    };

    struct dds_pixelformat_t
    {
        uint32_t dw_size;
        uint32_t dw_flags;
        uint32_t dw_fourcc;
        uint32_t dw_rgb_bit_count;
        uint32_t dw_rbitmask;
        uint32_t dw_gbitmask;
        uint32_t dw_bbitmask;
        uint32_t dw_abitmask;
    };
    static_assert(sizeof(dds_pixelformat_t) == 32, "DDS pixel format size mismatch");

    enum DDSD_CAPS
    {
        DDSD_CAPS = 0x1,			// Required in every .dds file.
        DDSD_HEIGHT = 0x2,			// Required in every .dds file.
        DDSD_WIDTH = 0x4,			// Required in every .dds file.
        DDSD_PITCH = 0x8,			// Required when pitch is provided for an uncompressed texture.
        DDSD_PIXELFORMAT = 0x1000,	// Required in every .dds file.
        DDSD_MIPMAPCOUNT = 0x20000,	// Required in a mipmapped texture.
        DDSD_LINEARSIZE = 0x80000,	// Required when pitch is provided for a compressed texture.
        DDSD_DEPTH = 0x800000		// Required in a depth texture.
    };

    enum DDSCAPS
    {
        DDSCAPS_COMPLEX = 0x8,		// Optional; must be used on any file that contains more than one surface (a mipmap, a cubic environment map, or mipmapped volume texture).
        DDSCAPS_MIPMAP = 0x400000,	// Optional; should be used for a mipmap.
        DDSCAPS_TEXTURE = 0x1000,	// Required
    };
    
    enum DDSCAPS2
    {
        DDSCAPS2_CUBEMAP = 0x200,			// Required for a cube map.
        DDSCAPS2_CUBEMAP_POSITIVEX = 0x400,	// Required when these surfaces are stored in a cube map.
        DDSCAPS2_CUBEMAP_NEGATIVEX = 0x800,	// Required when these surfaces are stored in a cube map.
        DDSCAPS2_CUBEMAP_POSITIVEY = 0x1000,// Required when these surfaces are stored in a cube map.
        DDSCAPS2_CUBEMAP_NEGATIVEY = 0x2000,// Required when these surfaces are stored in a cube map.
        DDSCAPS2_CUBEMAP_POSITIVEZ = 0x4000,// Required when these surfaces are stored in a cube map.
        DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x8000,// Required when these surfaces are stored in a cube map.
        DDSCAPS2_VOLUME = 0x200000,			// Required for a volume texture.
    };

    struct dds_header_t
    {
        uint32_t dw_size;
        uint32_t dw_flags;
        uint32_t dw_height;
        uint32_t dw_width;
        uint32_t dw_pitch_or_linear_size;
        uint32_t dw_depth;
        uint32_t dw_mipmap_count;
        uint32_t dw_reserved1[11];
        dds_pixelformat_t ddspf;
        uint32_t dw_caps;
        uint32_t dw_caps2;
        uint32_t dw_caps3;
        uint32_t dw_caps4;
        uint32_t dw_reserved2;
    };
    static_assert(sizeof(dds_header_t) == 124, "DDS header size mismatch");
    
    enum D3D10_RESOURCE_DIMENSION
    {
        D3D10_RESOURCE_DIMENSION_UNKNOWN = 0,
        D3D10_RESOURCE_DIMENSION_BUFFER = 1,
        D3D10_RESOURCE_DIMENSION_TEXTURE1D = 2,
        D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3,
        D3D10_RESOURCE_DIMENSION_TEXTURE3D = 4
    };
    
    struct dds_header_dxt10_t
    {
        DXGI_FORMAT dxgi_format;
        D3D10_RESOURCE_DIMENSION resource_dim;
        uint32_t misc_flags;
        uint32_t array_size;
        uint32_t misc_flags2;
    };
    static_assert(sizeof(dds_header_dxt10_t) == 20, "DDS header DXT10 size mismatch");
    
    enum DDS_RESOURCE_MISC_TEXTURECUBE
    {
        DDS_RESOURCE_MISC_TEXTURECUBE = 0x4,	// Indicates a 2D texture is a cube-map texture.
    };
    
    enum DDS_ALPHA_MODE
    {
        DDS_ALPHA_MODE_UNKNOWN = 0x0,		// Alpha channel content is unknown. This is the value for legacy files, which typically is assumed to be 'straight' alpha.
        DDS_ALPHA_MODE_STRAIGHT = 0x1,		// Any alpha channel content is presumed to use straight alpha.
        DDS_ALPHA_MODE_PREMULTIPLIED = 0x2,	// Any alpha channel content is using premultiplied alpha. The only legacy file formats that indicate this information are 'DX2' and 'DX4'.
        DDS_ALPHA_MODE_OPAQUE = 0x3,		// Any alpha channel content is all set to fully opaque.
        DDS_ALPHA_MODE_CUSTOM = 0x4,		// Any alpha channel content is being used as a 4th channel and is not intended to represent transparency (straight or premultiplied).
    };

    inline uint32_t fourcc(char a, char b, char c, char d)
    {
        return (((uint8_t)(d) << 24) | ((uint8_t)(c) << 16) | ((uint8_t)(b) << 8) | (uint8_t)(a));
    }
    
    inline bool ddspf_bitmask_compare(const dds_pixelformat_t& ddspf, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
    {
        return ddspf.dw_rbitmask == r && ddspf.dw_gbitmask == g && ddspf.dw_bbitmask == b && ddspf.dw_abitmask == a;
    }

    inline TEXTURE_FORMAT dxgi_to_texture_format(DXGI_FORMAT dxgi_format, bool srgb)
    {
        switch (dxgi_format)
        {
            case DXGI_FORMAT_R8G8_UNORM:		    return TEXTURE_FORMAT_RG8;
            case DXGI_FORMAT_R8G8B8A8_UNORM:	    return srgb ? TEXTURE_FORMAT_RGBA8_SRGB : TEXTURE_FORMAT_RGBA8;
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:	return TEXTURE_FORMAT_RGBA8_SRGB;
            case DXGI_FORMAT_R32G32B32A32_FLOAT:	return TEXTURE_FORMAT_RGBA32_FLOAT;
			    
            case DXGI_FORMAT_BC1_UNORM:			    return srgb ? TEXTURE_FORMAT_BC1_SRGB : TEXTURE_FORMAT_BC1;
            case DXGI_FORMAT_BC1_UNORM_SRGB:		return TEXTURE_FORMAT_BC1_SRGB;
            case DXGI_FORMAT_BC2_UNORM:			    return srgb ? TEXTURE_FORMAT_BC2_SRGB : TEXTURE_FORMAT_BC2;
            case DXGI_FORMAT_BC2_UNORM_SRGB:		return TEXTURE_FORMAT_BC2_SRGB;
            case DXGI_FORMAT_BC3_UNORM:			    return srgb ? TEXTURE_FORMAT_BC3_SRGB : TEXTURE_FORMAT_BC3;
            case DXGI_FORMAT_BC3_UNORM_SRGB:		return TEXTURE_FORMAT_BC3_SRGB;
            case DXGI_FORMAT_BC4_UNORM:			    return TEXTURE_FORMAT_BC4;
            case DXGI_FORMAT_BC5_UNORM:			    return TEXTURE_FORMAT_BC5;
            case DXGI_FORMAT_BC7_UNORM:			    return srgb ? TEXTURE_FORMAT_BC7_SRGB : TEXTURE_FORMAT_BC7;
            case DXGI_FORMAT_BC7_UNORM_SRGB:		return TEXTURE_FORMAT_BC7_SRGB;
			    
            default:							    return TEXTURE_FORMAT_UNKNOWN;
        }
    }

    struct header_t
    {
        uint32_t magic;
        dds_header_t dds;
        dds_header_dxt10_t dxt10;
    };

    inline bool header_is_valid(const header_t& header)
    {
        return
            header.magic == fourcc('D', 'D', 'S', ' ') &&
            header.dds.ddspf.dw_size == sizeof(dds_pixelformat_t);
    }

    inline bool header_has_dxt10(const header_t& header)
    {
        return
            header.dds.ddspf.dw_flags == DDPF_FOURCC &&
            header.dds.ddspf.dw_fourcc == fourcc('D', 'X', '1', '0');
    }

    inline uint32_t header_get_width(const header_t& header)
    {
        return header.dds.dw_width < 1 ? 1 : header.dds.dw_width;
    }

    inline uint32_t header_get_height(const header_t& header)
    {
        return header.dds.dw_height < 1 ? 1 : header.dds.dw_height;
    }

    inline uint32_t header_get_depth(const header_t& header)
    {
        return header.dds.dw_depth < 1 ? 1 : header.dds.dw_depth;
    }

    inline uint32_t header_get_mip_count(const header_t& header)
    {
        return header.dds.dw_mipmap_count < 1 ? 1 : header.dds.dw_mipmap_count;
    }

    inline bool header_is_cubemap(const header_t& header)
    {
        if (header_has_dxt10(header))
        {
            return IS_BIT_FLAG_SET(header.dxt10.misc_flags, DDS_RESOURCE_MISC_TEXTURECUBE);
        }

        return
            (IS_BIT_FLAG_SET(header.dds.dw_caps2, DDSCAPS2_CUBEMAP)) &&
            (IS_BIT_FLAG_SET(header.dds.dw_caps2, DDSCAPS2_CUBEMAP_POSITIVEX)) &&
            (IS_BIT_FLAG_SET(header.dds.dw_caps2, DDSCAPS2_CUBEMAP_NEGATIVEX)) &&
            (IS_BIT_FLAG_SET(header.dds.dw_caps2, DDSCAPS2_CUBEMAP_POSITIVEY)) &&
            (IS_BIT_FLAG_SET(header.dds.dw_caps2, DDSCAPS2_CUBEMAP_NEGATIVEY)) &&
            (IS_BIT_FLAG_SET(header.dds.dw_caps2, DDSCAPS2_CUBEMAP_POSITIVEZ)) &&
            (IS_BIT_FLAG_SET(header.dds.dw_caps2, DDSCAPS2_CUBEMAP_NEGATIVEZ));
    }

    inline uint32_t header_get_array_size(const header_t& header)
    {
        if (!header_has_dxt10(header))
        {
            if (header_is_cubemap(header))
            {
                return 6;
            }
            return 1;
        }

        uint32_t count = 0;
        if (header_is_cubemap(header))
        {
            count = header.dxt10.array_size * 6;
        }
        else
        {
            count = header.dxt10.array_size;
        }

        count = count < 1 ? 1 : count;
        return count;
    }

    inline DXGI_FORMAT header_get_format(const header_t& header)
    {
        const dds_pixelformat_t& ddspf = header.dds.ddspf;
        
        if (!header_has_dxt10(header))
        {
            if (IS_BIT_FLAG_SET(ddspf.dw_flags, DDPF_RGB))
            {
                switch (ddspf.dw_rgb_bit_count)
                {
                case 32:
                    if (ddspf_bitmask_compare(ddspf, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000)) return DXGI_FORMAT_R8G8B8A8_UNORM;
                    if (ddspf_bitmask_compare(ddspf, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000)) return DXGI_FORMAT_B8G8R8A8_UNORM;
                    if (ddspf_bitmask_compare(ddspf, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000)) return DXGI_FORMAT_B8G8R8X8_UNORM;
                    if (ddspf_bitmask_compare(ddspf, 0x0000ffff, 0xffff0000, 0x00000000, 0x00000000)) return DXGI_FORMAT_R16G16_UNORM;
                    if (ddspf_bitmask_compare(ddspf, 0xffffffff, 0x00000000, 0x00000000, 0x00000000)) return DXGI_FORMAT_R32_FLOAT;
                    break;
                case 24:
                    return DXGI_FORMAT_UNKNOWN; // D3DFMT_R8G8B8 does not exist in DXGI_FORMAT
                case 16:
                    if (ddspf_bitmask_compare(ddspf, 0x7c00, 0x03e0, 0x001f, 0x800)) return DXGI_FORMAT_B5G5R5A1_UNORM;
                    if (ddspf_bitmask_compare(ddspf, 0xf800, 0x07e0, 0x001f, 0x0000)) return DXGI_FORMAT_B5G6R5_UNORM;
                    if (ddspf_bitmask_compare(ddspf, 0x0f00, 0x00f0, 0x000f, 0xf000)) return DXGI_FORMAT_B4G4R4A4_UNORM;
                    break;
                default:
                    return DXGI_FORMAT_UNKNOWN;
                }
            }
            else if (IS_BIT_FLAG_SET(ddspf.dw_flags, DDPF_LUMINANCE))
            {
                if (ddspf.dw_rgb_bit_count == 8)
                {
                    if (ddspf_bitmask_compare(ddspf, 0x000000ff, 0x00000000, 0x00000000, 0x00000000)) return DXGI_FORMAT_R8_UNORM;
                    if (ddspf_bitmask_compare(ddspf, 0x000000ff, 0x0000ff00, 0x00000000, 0x00000000)) return DXGI_FORMAT_R8G8_UNORM;
                }
                else if (ddspf.dw_rgb_bit_count == 16)
                {
                    if (ddspf_bitmask_compare(ddspf, 0x0000ffff, 0x00000000, 0x00000000, 0x00000000)) return DXGI_FORMAT_R16_UNORM;
                    if (ddspf_bitmask_compare(ddspf, 0x000000ff, 0x0000ff00, 0x00000000, 0x00000000)) return DXGI_FORMAT_R8G8_UNORM;
                }
            }
            else if (IS_BIT_FLAG_SET(ddspf.dw_flags, DDPF_ALPHA))
            {
                if (ddspf.dw_rgb_bit_count == 8) return DXGI_FORMAT_A8_UNORM;
            }
            else if (IS_BIT_FLAG_SET(ddspf.dw_flags, DDPF_FOURCC))
            {
                if (ddspf.dw_fourcc == fourcc('D', 'X', 'T', '1')) return DXGI_FORMAT_BC1_UNORM;
                if (ddspf.dw_fourcc == fourcc('D', 'X', 'T', '3')) return DXGI_FORMAT_BC2_UNORM;
                if (ddspf.dw_fourcc == fourcc('D', 'X', 'T', '4')) return DXGI_FORMAT_BC2_UNORM;
                if (ddspf.dw_fourcc == fourcc('D', 'X', 'T', '5')) return DXGI_FORMAT_BC3_UNORM;

                if (ddspf.dw_fourcc == fourcc('A', 'T', 'I', '1')) return DXGI_FORMAT_BC4_UNORM;
                if (ddspf.dw_fourcc == fourcc('B', 'C', '4', 'U')) return DXGI_FORMAT_BC4_UNORM;
                if (ddspf.dw_fourcc == fourcc('B', 'C', '4', 'S')) return DXGI_FORMAT_BC4_SNORM;
                
                if (ddspf.dw_fourcc == fourcc('A', 'T', 'I', '2')) return DXGI_FORMAT_BC5_UNORM;
                if (ddspf.dw_fourcc == fourcc('B', 'C', '5', 'U')) return DXGI_FORMAT_BC5_UNORM;
                if (ddspf.dw_fourcc == fourcc('B', 'C', '5', 'S')) return DXGI_FORMAT_BC5_SNORM;
                
                if (ddspf.dw_fourcc == fourcc('R', 'G', 'B', 'G')) return DXGI_FORMAT_R8G8_B8G8_UNORM;
                if (ddspf.dw_fourcc == fourcc('G', 'R', 'G', 'B')) return DXGI_FORMAT_G8R8_G8B8_UNORM;
                if (ddspf.dw_fourcc == fourcc('Y', 'U', 'Y', '2')) return DXGI_FORMAT_YUY2;

                switch (ddspf.dw_fourcc)
                {
                case 36:  return DXGI_FORMAT_R16G16B16A16_UNORM;
                case 110: return DXGI_FORMAT_R16G16B16A16_SNORM;
                case 111: return DXGI_FORMAT_R16_FLOAT;
                case 112: return DXGI_FORMAT_R16G16_FLOAT;
                case 113: return DXGI_FORMAT_R16G16B16A16_FLOAT;
                case 114: return DXGI_FORMAT_R32_FLOAT;
                case 115: return DXGI_FORMAT_R32G32_FLOAT;
                case 116: return DXGI_FORMAT_R32G32B32A32_FLOAT;
                }
            }

            return DXGI_FORMAT_UNKNOWN;
        }

        return header.dxt10.dxgi_format;
    }

    inline uint32_t header_get_bits_per_element(const header_t& header)
    {
        switch (header_get_format(header))
        {
            case DXGI_FORMAT_R32G32B32A32_TYPELESS:
			case DXGI_FORMAT_R32G32B32A32_FLOAT:
			case DXGI_FORMAT_R32G32B32A32_UINT:
			case DXGI_FORMAT_R32G32B32A32_SINT:
				return 128;

			case DXGI_FORMAT_R32G32B32_TYPELESS:
			case DXGI_FORMAT_R32G32B32_FLOAT:
			case DXGI_FORMAT_R32G32B32_UINT:
			case DXGI_FORMAT_R32G32B32_SINT:
				return 96;

			case DXGI_FORMAT_R16G16B16A16_TYPELESS:
			case DXGI_FORMAT_R16G16B16A16_FLOAT:
			case DXGI_FORMAT_R16G16B16A16_UNORM:
			case DXGI_FORMAT_R16G16B16A16_UINT:
			case DXGI_FORMAT_R16G16B16A16_SNORM:
			case DXGI_FORMAT_R16G16B16A16_SINT:
			case DXGI_FORMAT_R32G32_TYPELESS:
			case DXGI_FORMAT_R32G32_FLOAT:
			case DXGI_FORMAT_R32G32_UINT:
			case DXGI_FORMAT_R32G32_SINT:
			case DXGI_FORMAT_R32G8X24_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			case DXGI_FORMAT_Y416:
			case DXGI_FORMAT_Y210:
			case DXGI_FORMAT_Y216:
				return 64;

			case DXGI_FORMAT_R10G10B10A2_TYPELESS:
			case DXGI_FORMAT_R10G10B10A2_UNORM:
			case DXGI_FORMAT_R10G10B10A2_UINT:
			case DXGI_FORMAT_R11G11B10_FLOAT:
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			case DXGI_FORMAT_R8G8B8A8_UINT:
			case DXGI_FORMAT_R8G8B8A8_SNORM:
			case DXGI_FORMAT_R8G8B8A8_SINT:
			case DXGI_FORMAT_R16G16_TYPELESS:
			case DXGI_FORMAT_R16G16_FLOAT:
			case DXGI_FORMAT_R16G16_UNORM:
			case DXGI_FORMAT_R16G16_UINT:
			case DXGI_FORMAT_R16G16_SNORM:
			case DXGI_FORMAT_R16G16_SINT:
			case DXGI_FORMAT_R32_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT:
			case DXGI_FORMAT_R32_FLOAT:
			case DXGI_FORMAT_R32_UINT:
			case DXGI_FORMAT_R32_SINT:
			case DXGI_FORMAT_R24G8_TYPELESS:
			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
			case DXGI_FORMAT_R8G8_B8G8_UNORM:
			case DXGI_FORMAT_G8R8_G8B8_UNORM:
			case DXGI_FORMAT_B8G8R8A8_UNORM:
			case DXGI_FORMAT_B8G8R8X8_UNORM:
			case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
			case DXGI_FORMAT_B8G8R8A8_TYPELESS:
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			case DXGI_FORMAT_B8G8R8X8_TYPELESS:
			case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			case DXGI_FORMAT_AYUV:
			case DXGI_FORMAT_Y410:
			case DXGI_FORMAT_YUY2:
				return 32;

			case DXGI_FORMAT_P010:
			case DXGI_FORMAT_P016:
				return 24;

			case DXGI_FORMAT_R8G8_TYPELESS:
			case DXGI_FORMAT_R8G8_UNORM:
			case DXGI_FORMAT_R8G8_UINT:
			case DXGI_FORMAT_R8G8_SNORM:
			case DXGI_FORMAT_R8G8_SINT:
			case DXGI_FORMAT_R16_TYPELESS:
			case DXGI_FORMAT_R16_FLOAT:
			case DXGI_FORMAT_D16_UNORM:
			case DXGI_FORMAT_R16_UNORM:
			case DXGI_FORMAT_R16_UINT:
			case DXGI_FORMAT_R16_SNORM:
			case DXGI_FORMAT_R16_SINT:
			case DXGI_FORMAT_B5G6R5_UNORM:
			case DXGI_FORMAT_B5G5R5A1_UNORM:
			case DXGI_FORMAT_A8P8:
			case DXGI_FORMAT_B4G4R4A4_UNORM:
				return 16;

			case DXGI_FORMAT_NV12:
			case DXGI_FORMAT_NV11:
				return 12;

			case DXGI_FORMAT_R8_TYPELESS:
			case DXGI_FORMAT_R8_UNORM:
			case DXGI_FORMAT_R8_UINT:
			case DXGI_FORMAT_R8_SNORM:
			case DXGI_FORMAT_R8_SINT:
			case DXGI_FORMAT_A8_UNORM:
			case DXGI_FORMAT_AI44:
			case DXGI_FORMAT_IA44:
			case DXGI_FORMAT_P8:
				return 8;

			case DXGI_FORMAT_R1_UNORM:
				return 1;

			case DXGI_FORMAT_BC1_TYPELESS:
			case DXGI_FORMAT_BC1_UNORM:
			case DXGI_FORMAT_BC1_UNORM_SRGB:
			case DXGI_FORMAT_BC4_TYPELESS:
			case DXGI_FORMAT_BC4_UNORM:
			case DXGI_FORMAT_BC4_SNORM:
				return 64;

			case DXGI_FORMAT_BC2_TYPELESS:
			case DXGI_FORMAT_BC2_UNORM:
			case DXGI_FORMAT_BC2_UNORM_SRGB:
			case DXGI_FORMAT_BC3_TYPELESS:
			case DXGI_FORMAT_BC3_UNORM:
			case DXGI_FORMAT_BC3_UNORM_SRGB:
			case DXGI_FORMAT_BC5_TYPELESS:
			case DXGI_FORMAT_BC5_UNORM:
			case DXGI_FORMAT_BC5_SNORM:
			case DXGI_FORMAT_BC6H_TYPELESS:
			case DXGI_FORMAT_BC6H_UF16:
			case DXGI_FORMAT_BC6H_SF16:
			case DXGI_FORMAT_BC7_TYPELESS:
			case DXGI_FORMAT_BC7_UNORM:
			case DXGI_FORMAT_BC7_UNORM_SRGB:
				return 128;
            
			default:
				return 0;
        }
    }

    inline size_t header_get_bits_per_pixel(const header_t& header)
    {
        switch (header_get_format(header))
        {
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
            return 128;

        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:
            return 96;

        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R32G32_TYPELESS:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        case DXGI_FORMAT_Y416:
        case DXGI_FORMAT_Y210:
        case DXGI_FORMAT_Y216:
            return 64;

        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT:
        case DXGI_FORMAT_R16G16_TYPELESS:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SNORM:
        case DXGI_FORMAT_R16G16_SINT:
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
        case DXGI_FORMAT_R8G8_B8G8_UNORM:
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        case DXGI_FORMAT_AYUV:
        case DXGI_FORMAT_Y410:
        case DXGI_FORMAT_YUY2:
            return 32;

        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_P016:
            return 24;

        case DXGI_FORMAT_R8G8_TYPELESS:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SNORM:
        case DXGI_FORMAT_R8G8_SINT:
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SNORM:
        case DXGI_FORMAT_R16_SINT:
        case DXGI_FORMAT_B5G6R5_UNORM:
        case DXGI_FORMAT_B5G5R5A1_UNORM:
        case DXGI_FORMAT_A8P8:
        case DXGI_FORMAT_B4G4R4A4_UNORM:
            return 16;

        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_420_OPAQUE:
        case DXGI_FORMAT_NV11:
            return 12;

        case DXGI_FORMAT_R8_TYPELESS:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_SINT:
        case DXGI_FORMAT_A8_UNORM:
        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
        case DXGI_FORMAT_AI44:
        case DXGI_FORMAT_IA44:
        case DXGI_FORMAT_P8:
            return 8;

        case DXGI_FORMAT_R1_UNORM:
            return 1;

        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            return 4;

        case DXGI_FORMAT_UNKNOWN:
        case DXGI_FORMAT_FORCE_UINT:
        default:
            return 0;
        }
    }

    inline uint32_t header_get_block_size(const header_t& header)
    {
        switch (header_get_format(header))
        {
            case DXGI_FORMAT_BC1_TYPELESS:
            case DXGI_FORMAT_BC1_UNORM:
            case DXGI_FORMAT_BC1_UNORM_SRGB:
            case DXGI_FORMAT_BC4_TYPELESS:
            case DXGI_FORMAT_BC4_UNORM:
            case DXGI_FORMAT_BC4_SNORM:
            case DXGI_FORMAT_BC2_TYPELESS:
            case DXGI_FORMAT_BC2_UNORM:
            case DXGI_FORMAT_BC2_UNORM_SRGB:
            case DXGI_FORMAT_BC3_TYPELESS:
            case DXGI_FORMAT_BC3_UNORM:
            case DXGI_FORMAT_BC3_UNORM_SRGB:
            case DXGI_FORMAT_BC5_TYPELESS:
            case DXGI_FORMAT_BC5_UNORM:
            case DXGI_FORMAT_BC5_SNORM:
            case DXGI_FORMAT_BC6H_TYPELESS:
            case DXGI_FORMAT_BC6H_UF16:
            case DXGI_FORMAT_BC6H_SF16:
            case DXGI_FORMAT_BC7_TYPELESS:
            case DXGI_FORMAT_BC7_UNORM:
            case DXGI_FORMAT_BC7_UNORM_SRGB:
                return 4;
            
            default:
                return 1;
        }
    }

    inline uint64_t header_get_mip_size(const header_t& header, uint32_t mip)
    {
        const uint64_t bits_per_element = header_get_bits_per_element(header);
        const uint64_t block_size = header_get_block_size(header);

        uint64_t element_count_x = header_get_width(header);
        uint64_t element_count_y = header_get_height(header);
        uint64_t element_count_z = header_get_depth(header);

        element_count_x >>= mip;
        element_count_y >>= mip;
        element_count_z >>= mip;

        element_count_x = element_count_x < 1 ? 1 : element_count_x;
        element_count_y = element_count_y < 1 ? 1 : element_count_y;
        element_count_z = element_count_z < 1 ? 1 : element_count_z;

        const uint64_t block_count_x = (element_count_x + block_size - 1) / block_size;
        const uint64_t block_count_y = (element_count_y + block_size - 1) / block_size;

        return block_count_x * block_count_y * element_count_z * bits_per_element / 8u;
    }

    inline uint64_t header_get_slice_size(const header_t& header)
    {
        const uint32_t mip_count = header_get_mip_count(header);
        uint64_t size = 0;

        for (uint32_t mip = 0; mip < mip_count; ++mip)
        {
            size += header_get_mip_size(header, mip);
        }

        return size;
    }

    inline uint64_t header_get_data_size(const header_t& header)
    {
        return header_get_array_size(header) * header_get_slice_size(header);
    }

    inline uint64_t header_get_data_offset(const header_t& header)
    {
        uint64_t offset = sizeof(header_t);
        if (!header_has_dxt10(header))
        {
            offset -= sizeof(header_t::dxt10);
        }

        return offset;
    }

    inline uint64_t header_get_slice_offset(const header_t& header, uint32_t slice)
    {
        return header_get_data_offset(header) + header_get_slice_size(header) * slice;
    }

    inline uint64_t header_get_mip_offset(const header_t& header, uint32_t mip, uint32_t slice = 0)
    {
        uint64_t offset = header_get_slice_offset(header, slice);
        for (uint32_t i = 0; i < mip; ++i)
        {
            offset += header_get_mip_size(header, i);
        }

        return offset;
    }

    inline uint64_t header_get_row_pitch(const header_t& header, uint32_t mip)
    {
        const uint64_t bits_per_element = header_get_bits_per_element(header);
        const uint64_t block_size = header_get_block_size(header);

        uint64_t element_count_x = header_get_width(header);
        element_count_x >>= mip;
        element_count_x = element_count_x < 1 ? 1 : element_count_x;

        const uint64_t block_count_x = (element_count_x + block_size - 1) / block_size;
        return block_count_x * bits_per_element / 8ull;
    }

    inline uint64_t header_get_slice_pitch(const header_t& header, uint32_t mip)
    {
        const uint64_t bits_per_element = header_get_bits_per_element(header);
        const uint64_t block_size = header_get_block_size(header);
        
        uint64_t element_count_x = header_get_width(header);
        uint64_t element_count_y = header_get_height(header);
        
        element_count_x >>= mip;
        element_count_y >>= mip;
        
        element_count_x = element_count_x < 1 ? 1 : element_count_x;
        element_count_y = element_count_y < 1 ? 1 : element_count_y;

        const uint64_t block_count_x = (element_count_x + block_size - 1) / block_size;
        const uint64_t block_count_y = (element_count_y + block_size - 1) / block_size;

        return block_count_x * block_count_y * bits_per_element / 8ull;
    }

    inline header_t header_read(const void* data, uint64_t size)
    {
        header_t header = {};
        
        if (data == nullptr)
        {
            return header;
        }
        if (size < sizeof(header_t::magic) + sizeof(header_t::dds))
        {
            return header;
        }

        header.magic = *(const uint32_t*)data;
        if (header.magic != fourcc('D', 'D', 'S', ' '))
        {
            return header;
        }

        header.dds = *(const dds_header_t*)((const uint8_t*)data + sizeof(header.magic));
        if (size >= sizeof(header) && header_has_dxt10(header))
        {
            header.dxt10 = (*(const header_t*)data).dxt10;
        }

        return header;
    }

    inline void header_write(void* dst, DXGI_FORMAT format, uint32_t width, uint32_t height,
        uint32_t mip_count = 1, uint32_t array_size = 1, bool is_cubemap = false, uint32_t depth = 0)
    {
        header_t header = {};
        header.magic = fourcc('D', 'D', 'S', ' ');
        header.dds.dw_size = sizeof(dds_header_t);
        header.dds.dw_flags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT;
        header.dds.dw_width = width;
        header.dds.dw_height = height;
        header.dds.dw_depth = depth;
        header.dds.dw_mipmap_count = mip_count;
        header.dds.ddspf.dw_size = sizeof(dds_pixelformat_t);
        header.dds.ddspf.dw_flags = DDPF_FOURCC;
        header.dds.ddspf.dw_fourcc = fourcc('D', 'X', '1', '0');
        header.dds.dw_caps = DDSCAPS_TEXTURE;

        header.dxt10.dxgi_format = format;
        header.dxt10.resource_dim = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
        header.dxt10.misc_flags2 = DDS_ALPHA_MODE_UNKNOWN;

        if (is_cubemap)
        {
            header.dds.dw_caps |= DDSCAPS_COMPLEX;
            header.dds.dw_caps2 = DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEX | DDSCAPS2_CUBEMAP_NEGATIVEX |
                DDSCAPS2_CUBEMAP_POSITIVEY | DDSCAPS2_CUBEMAP_NEGATIVEY | DDSCAPS2_CUBEMAP_POSITIVEZ | DDSCAPS2_CUBEMAP_NEGATIVEZ;

            header.dxt10.array_size = array_size / 6;
            header.dxt10.misc_flags = DDS_RESOURCE_MISC_TEXTURECUBE;
        }
        else if (depth > 0)
        {
            header.dds.dw_caps2 = DDSCAPS2_VOLUME;
            header.dxt10.array_size = 1;
            header.dxt10.resource_dim = D3D10_RESOURCE_DIMENSION_TEXTURE3D;
        }
        else
        {
            header.dxt10.array_size = array_size;
        }

        if (height == 0)
        {
            header.dds.dw_height = 1;
            header.dxt10.resource_dim = D3D10_RESOURCE_DIMENSION_TEXTURE1D;
        }

        if (mip_count > 1)
        {
            header.dds.dw_caps |= DDSCAPS_COMPLEX;
        }

        *(header_t*)dst = header;
    }
    
}
