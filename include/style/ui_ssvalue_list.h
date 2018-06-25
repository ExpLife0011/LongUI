﻿#pragma once

#include <cstdint>
#include <cassert>
#include <core/ui_core_type.h>


namespace LongUI {
    /// <summary>
    /// type of value
    /// </summary>
    enum class ValueType : uint32_t {
        // unknown
        Type_Unknown = 0,

        //// [Position] cursor 
        //Type_PositionCursor,
        //// [Position] left 
        //Type_PositionLeft,
        //// [Position] right 
        //Type_PositionRight,
        // [Position] overflow
        Type_PositionOverflow,
        // [Position] overflow-x
        Type_PositionOverflowX,
        // [Position] overflow-y
        Type_PositionOverflowY,
        //// [Position] z-index
        //Type_PositionZindex,

        // [Margin] margin
        Type_Margin,
        // [Margin] top
        Type_MarginTop,
        // [Margin] right
        Type_MarginRight,
        // [Margin] bottom
        Type_MarginBottom,
        // [Margin] left
        Type_MarginLeft,

        // [Padding] padding
        Type_Padding,
        // [Padding] top
        Type_PaddingTop,
        // [Padding] right
        Type_PaddingRight,
        // [Padding] bottom
        Type_PaddingBottom,
        // [Padding] left
        Type_PaddingLeft,

        // [Border] width
        Type_BorderWidth,
        // [Border] top-width
        Type_BorderTopWidth,
        // [Border] right-width
        Type_BorderRightWidth,
        // [Border] bottom-width
        Type_BorderBottomWidth,
        // [Border] left-width
        Type_BorderLeftWidth,
        //// [Border] top-style
        //Type_BorderTopStyle,
        //// [Border] right-style
        //Type_BorderRightStyle,
        //// [Border] bottom-style
        //Type_BorderBottomStyle,
        //// [Border] left-style
        //Type_BorderLeftStyle,
        //// [Border] top-color
        //Type_BorderTopColor,
        //// [Border] right-color
        //Type_BorderRightColor,
        //// [Border] bottom-color
        //Type_BorderBottomColor,
        //// [Border] left-color
        //Type_BorderLeftColor,
        //// [Border] top-left-radius
        //Type_BorderTopLeftRadius,
        //// [Border] top-right-radius
        //Type_BorderTopRightRadius,
        //// [Border] bottom-left-radius
        //Type_BorderBottomLeftRadius,
        //// [Border] bottom-right-radius
        //Type_BorderBottomRightRadius,
        // [Border] image-source
        Type_BorderImageSource,
        //// [Border] image-slice
        //Type_BorderBottomRightRadius,
        //// [Border] image-width
        //Type_BorderLeftWidth,
        
        // [Background] color
        Type_BackgroundColor,
        // [Background] image
        Type_BackgroundImage,
        // [Background] attachment
        Type_BackgroundAttachment,
        // [Background] repeat
        Type_BackgroundRepeat,
        // [Background] clip
        Type_BackgroundClip,
        // [Background] origin 
        Type_BackgroundOrigin,

        // [LongUI] appearance
        Type_UIAppearance,
    };
#if 0
    // extra value type
    struct ExValueType {
        // value type
        ValueType       vtype;
        // extra number
        //uint32_t        extra;
    };
#endif
    // u8view to value type
    auto U8View2ValueType(U8View view) noexcept->ValueType;
    // make value
    void ValueTypeMakeValue(
        ValueType ex, 
        uint32_t value_len,
        const U8View values[],
        void* values_write
    ) noexcept;
}
